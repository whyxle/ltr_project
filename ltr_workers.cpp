#include "ltr_workers.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QThread>
#include <algorithm>
#include <utility>

#include "ltr114.h"
#include "ltr212.h"

namespace {
constexpr DWORD kRecvTimeoutMs = 50;
constexpr int kModule114 = 0;
constexpr int kModule212 = 1;

quint32 extractStartMark(DWORD tmarkWord)
{
    return (tmarkWord >> 16) & 0xFFFF;
}

quint32 extractSecondMark(DWORD tmarkWord)
{
    return tmarkWord & 0xFFFF;
}

quint32 unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch)
{
    if (rawValue < lastRaw && static_cast<quint16>(lastRaw - rawValue) > 32768U)
        epoch += 65536U;

    lastRaw = rawValue;
    return epoch + static_cast<quint32>(rawValue);
}

TmarkPoint decodeTmark(DWORD tmarkWord, TmarkDecodeState& state)
{
    const quint16 rawStart = static_cast<quint16>(extractStartMark(tmarkWord));
    const quint16 rawSecond = static_cast<quint16>(extractSecondMark(tmarkWord));

    if (!state.initialized) {
        state.lastStartRaw = rawStart;
        state.lastSecondRaw = rawSecond;
        state.initialized = true;
        return {static_cast<quint32>(rawStart), static_cast<quint32>(rawSecond)};
    }

    return {unwrapCounter(rawStart, state.lastStartRaw, state.startEpoch),
            unwrapCounter(rawSecond, state.lastSecondRaw, state.secondEpoch)};
}

SyncModuleState& moduleState(SyncState& state, int moduleId)
{
    return moduleId == kModule212 ? state.ltr212 : state.ltr114;
}

QString moduleName(int moduleId)
{
    return moduleId == kModule212 ? QStringLiteral("LTR212") : QStringLiteral("LTR114");
}

double validSampleRate(double sampleRate)
{
    return sampleRate > 0.001 ? sampleRate : 1.0;
}

int proportionalIndex(int valueIndex, int valueCount, int timelineCount)
{
    if (timelineCount <= 1 || valueCount <= 1)
        return 0;

    const qint64 numerator = static_cast<qint64>(valueIndex) * static_cast<qint64>(timelineCount - 1);
    return static_cast<int>(numerator / static_cast<qint64>(valueCount - 1));
}

quint64 sampleTick(quint64 sampleIndex, double sampleRate, quint64 timeBaseTicks)
{
    const long double ticks = (static_cast<long double>(sampleIndex) * static_cast<long double>(timeBaseTicks))
                              / static_cast<long double>(validSampleRate(sampleRate));
    return static_cast<quint64>(ticks + 0.5L);
}

struct SyncDecision
{
    bool accept = false;
    bool failed = false;
    bool syncEnabled = false;
    bool running = true;
    bool readyForStart = false;
    bool started = false;
    quint32 start = 0;
    quint32 second = 0;
    quint64 dropped = 0;
    QString error;
};

SyncDecision decideSync(SyncState* syncState, int moduleId, const TmarkPoint& point)
{
    SyncDecision decision;
    decision.start = point.startMark;
    decision.second = point.secondMark;

    if (!syncState || !syncState->needSynchronization) {
        decision.accept = true;
        return decision;
    }

    const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker locker(&syncState->mutex);
    decision.syncEnabled = true;
    decision.running = false;

    if (syncState->state == SyncSessionState::Failed) {
        decision.failed = true;
        decision.error = syncState->failureMessage;
        return decision;
    }

    SyncModuleState& current = moduleState(*syncState, moduleId);
    SyncModuleState& other = moduleState(*syncState, moduleId == kModule212 ? kModule114 : kModule212);

    if (!current.baselineSeen) {
        current.baselineSeen = true;
        current.baselineStart = point.startMark;
        current.baselineSecond = point.secondMark;
        decision.readyForStart = true;
        ++current.droppedBeforeStart;
        return decision;
    }

    if (point.startMark < current.baselineStart || point.secondMark < current.baselineSecond) {
        syncState->state = SyncSessionState::Failed;
        syncState->failureMessage = QString("%1: impossible tmark sequence (START %2 -> %3, SECOND %4 -> %5)")
                                        .arg(moduleName(moduleId))
                                        .arg(current.baselineStart)
                                        .arg(point.startMark)
                                        .arg(current.baselineSecond)
                                        .arg(point.secondMark);
        decision.failed = true;
        decision.error = syncState->failureMessage;
        return decision;
    }

    if (!syncState->startMarkRequested) {
        current.baselineStart = point.startMark;
        current.baselineSecond = point.secondMark;
        ++current.droppedBeforeStart;
        decision.dropped = current.droppedBeforeStart;
        return decision;
    }

    if (!current.startSeen) {
        if (point.startMark <= current.baselineStart) {
            ++current.droppedBeforeStart;
            decision.dropped = current.droppedBeforeStart;

            if (syncState->startMarkRequestMsec > 0
                && nowMsec - syncState->startMarkRequestMsec > syncState->startTimeoutMs) {
                syncState->state = SyncSessionState::Failed;
                syncState->failureMessage = QString("%1: START mark was not received within %2 ms")
                                                .arg(moduleName(moduleId))
                                                .arg(syncState->startTimeoutMs);
                decision.failed = true;
                decision.error = syncState->failureMessage;
            }
            return decision;
        }

        current.startSeen = true;
        current.startMark = point.startMark;
        current.startSecond = point.secondMark;
        decision.started = true;
        decision.dropped = current.droppedBeforeStart;

        if (other.startSeen)
            syncState->state = SyncSessionState::Running;
    }

    decision.running = (syncState->state == SyncSessionState::Running);
    ++current.acceptedRaw;
    decision.accept = true;
    return decision;
}

QString checkSyncTimeout(SyncState* syncState, int moduleId)
{
    if (!syncState || !syncState->needSynchronization)
        return {};

    const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker locker(&syncState->mutex);

    if (syncState->state == SyncSessionState::Failed)
        return syncState->failureMessage;

    SyncModuleState& current = moduleState(*syncState, moduleId);

    if (syncState->state == SyncSessionState::WaitingBaseline
        && !current.baselineSeen
        && syncState->sessionStartMsec > 0
        && nowMsec - syncState->sessionStartMsec > syncState->baselineTimeoutMs) {
        syncState->state = SyncSessionState::Failed;
        syncState->failureMessage = QString("%1: baseline tmark was not received within %2 ms")
                                        .arg(moduleName(moduleId))
                                        .arg(syncState->baselineTimeoutMs);
        return syncState->failureMessage;
    }

    if (syncState->state == SyncSessionState::WaitingStartEdge
        && !current.startSeen
        && syncState->startMarkRequestMsec > 0
        && nowMsec - syncState->startMarkRequestMsec > syncState->startTimeoutMs) {
        syncState->state = SyncSessionState::Failed;
        syncState->failureMessage = QString("%1: START mark was not received within %2 ms")
                                        .arg(moduleName(moduleId))
                                        .arg(syncState->startTimeoutMs);
        return syncState->failureMessage;
    }

    return {};
}

void addAcceptedValues(SyncState* syncState, int moduleId, qsizetype count)
{
    if (!syncState || count <= 0)
        return;

    QMutexLocker locker(&syncState->mutex);
    moduleState(*syncState, moduleId).acceptedValues += static_cast<quint64>(count);
}

quint64 syncTimeBase(SyncState* syncState)
{
    if (!syncState)
        return 1000000ULL;

    QMutexLocker locker(&syncState->mutex);
    return syncState->timeBaseTicks;
}

} // namespace

Ltr114Worker::Ltr114Worker(LTR114* module, SyncState* syncState, QObject* parent)
    : QObject(parent)
    , m_module(module)
    , m_syncState(syncState)
{
}

void Ltr114Worker::run()
{
    if (!m_module) {
        emit acquisitionError("LTR114 worker: module is not initialized");
        emit finished();
        return;
    }

    if (!m_running.load()) {
        emit finished();
        return;
    }

    m_tmarkState = {};
    m_valueSampleIndex = 0;
    m_readySignalEmitted = false;
    m_startedSignalEmitted = false;
    m_pendingRaw.clear();
    m_pendingTimeline.clear();

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (!m_running.load())
            break;

        if (error != 0) {
            emit acquisitionError(QString("LTR114: receive error %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            const QString timeoutError = checkSyncTimeout(m_syncState, kModule114);
            if (!timeoutError.isEmpty()) {
                emit acquisitionError(timeoutError);
                break;
            }
            QThread::msleep(1);
            continue;
        }

        QVector<DWORD> acceptedRaw;
        const QVector<TimelinePoint> timeline = buildTimeline(rawData, tmarks, &acceptedRaw);
        if (!m_running.load())
            break;

        if (timeline.isEmpty() || acceptedRaw.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        QVector<DWORD> dataToProcess = acceptedRaw;
        QVector<double> procData(dataToProcess.size());
        QVector<double> thermData(dataToProcess.size());
        INT procSize = dataToProcess.size();
        INT thermCount = 0;

        const INT result = LTR114_ProcessDataTherm(m_module->handle(),
                                                   dataToProcess.data(),
                                                   procData.data(),
                                                   thermData.data(),
                                                   &procSize,
                                                   &thermCount,
                                                   LTR114_CORRECTION_MODE_INIT,
                                                   LTR114_PROCF_VALUE);

        if (!m_running.load())
            break;

        if (result != LTR_OK) {
            emit acquisitionError(QString("LTR114: process error %1").arg(result));
            break;
        }

        procData.resize(procSize);
        const QVector<TimedSample> timed = attachTimeline(procData, timeline);
        if (!timed.isEmpty())
            emit newVoltageSamples(timed);

        QThread::msleep(1);
    }

    m_running.store(false);
    emit finished();
}

void Ltr114Worker::stopAcquisition()
{
    m_running.store(false);
}

QVector<TimelinePoint> Ltr114Worker::buildTimeline(const QVector<DWORD>& rawData,
                                                   const QVector<DWORD>& tmarks,
                                                   QVector<DWORD>* acceptedRaw)
{
    QVector<TimelinePoint> timeline;
    if (!acceptedRaw || rawData.isEmpty())
        return timeline;

    acceptedRaw->clear();
    acceptedRaw->reserve(rawData.size());
    timeline.reserve(rawData.size());

    for (int i = 0; i < rawData.size(); ++i) {
        const DWORD tmarkWord = (i < tmarks.size()) ? tmarks[i] : 0;
        const TmarkPoint point = decodeTmark(tmarkWord, m_tmarkState);
        const SyncDecision decision = decideSync(m_syncState, kModule114, point);

        if (decision.readyForStart && !m_readySignalEmitted) {
            m_readySignalEmitted = true;
            emit syncReadyForStart(kModule114, decision.start, decision.second);
        }

        if (decision.started && !m_startedSignalEmitted) {
            m_startedSignalEmitted = true;
            emit syncStarted(kModule114, decision.start, decision.second, decision.dropped);
        }

        if (decision.failed) {
            emit acquisitionError(decision.error);
            m_running.store(false);
            return {};
        }

        if (!decision.accept)
            continue;

        const TimelinePoint timelinePoint{point.startMark, point.secondMark};
        if (decision.syncEnabled && !decision.running) {
            m_pendingRaw.append(rawData[i]);
            m_pendingTimeline.append(timelinePoint);
            continue;
        }

        if (!m_pendingRaw.isEmpty()) {
            for (DWORD pendingWord : m_pendingRaw)
                acceptedRaw->append(pendingWord);
            for (const TimelinePoint& pendingPoint : m_pendingTimeline)
                timeline.append(pendingPoint);
            m_pendingRaw.clear();
            m_pendingTimeline.clear();
        }

        acceptedRaw->append(rawData[i]);
        timeline.append(timelinePoint);
    }

    return timeline;
}

QVector<TimedSample> Ltr114Worker::attachTimeline(const QVector<double>& values,
                                                  const QVector<TimelinePoint>& timeline)
{
    QVector<TimedSample> result;
    if (values.isEmpty() || timeline.isEmpty())
        return result;

    const double sampleRate = validSampleRate(static_cast<double>(LTR114_FREQ(*m_module->handle())));
    const quint64 timeBase = syncTimeBase(m_syncState);
    const quint64 samplesPerSecond = qMax<quint64>(1ULL, static_cast<quint64>(sampleRate + 0.5));

    result.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        const int idx = proportionalIndex(i, values.size(), timeline.size());
        const quint64 sampleIndex = m_valueSampleIndex++;

        TimedSample sample;
        sample.globalTick = sampleTick(sampleIndex, sampleRate, timeBase);
        sample.startMark = timeline[idx].startMark;
        sample.secondMark = timeline[idx].secondMark;
        sample.sampleInSecond = static_cast<quint32>(sampleIndex % samplesPerSecond);
        sample.value = values[i];
        result.append(sample);
    }

    addAcceptedValues(m_syncState, kModule114, result.size());
    return result;
}

Ltr212Worker::Ltr212Worker(LTR212* module, SyncState* syncState, QObject* parent)
    : QObject(parent)
    , m_module(module)
    , m_syncState(syncState)
{
}

void Ltr212Worker::run()
{
    if (!m_module) {
        emit acquisitionError("LTR212 worker: module is not initialized");
        emit finished();
        return;
    }

    if (!m_running.load()) {
        emit finished();
        return;
    }

    m_tmarkState = {};
    m_valueSampleIndex = 0;
    m_readySignalEmitted = false;
    m_startedSignalEmitted = false;
    m_pendingRaw.clear();
    m_pendingTimeline.clear();

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (!m_running.load())
            break;

        if (error != 0) {
            emit acquisitionError(QString("LTR212: receive error %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            const QString timeoutError = checkSyncTimeout(m_syncState, kModule212);
            if (!timeoutError.isEmpty()) {
                emit acquisitionError(timeoutError);
                break;
            }
            QThread::msleep(1);
            continue;
        }

        QVector<DWORD> acceptedRaw;
        const QVector<TimelinePoint> timeline = buildTimeline(rawData, tmarks, &acceptedRaw);
        if (!m_running.load())
            break;

        if (timeline.isEmpty() || acceptedRaw.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<DWORD> dataToProcess = acceptedRaw;
        const QVector<double> voltageSamples = m_module->process_data(dataToProcess, true);
        if (!m_running.load())
            break;

        const QVector<TimedSample> timed = remapAndAttachTimeline(voltageSamples, timeline);
        if (!timed.isEmpty())
            emit newVoltageSamples(timed);

        QThread::msleep(1);
    }

    m_running.store(false);
    emit finished();
}

void Ltr212Worker::stopAcquisition()
{
    m_running.store(false);
}

QVector<TimelinePoint> Ltr212Worker::buildTimeline(const QVector<DWORD>& rawData,
                                                   const QVector<DWORD>& tmarks,
                                                   QVector<DWORD>* acceptedRaw)
{
    QVector<TimelinePoint> timeline;
    if (!acceptedRaw || rawData.isEmpty())
        return timeline;

    acceptedRaw->clear();
    acceptedRaw->reserve(rawData.size());
    timeline.reserve(rawData.size());

    for (int i = 0; i < rawData.size(); ++i) {
        const DWORD tmarkWord = (i < tmarks.size()) ? tmarks[i] : 0;
        const TmarkPoint point = decodeTmark(tmarkWord, m_tmarkState);
        const SyncDecision decision = decideSync(m_syncState, kModule212, point);

        if (decision.readyForStart && !m_readySignalEmitted) {
            m_readySignalEmitted = true;
            emit syncReadyForStart(kModule212, decision.start, decision.second);
        }

        if (decision.started && !m_startedSignalEmitted) {
            m_startedSignalEmitted = true;
            emit syncStarted(kModule212, decision.start, decision.second, decision.dropped);
        }

        if (decision.failed) {
            emit acquisitionError(decision.error);
            m_running.store(false);
            return {};
        }

        if (!decision.accept)
            continue;

        const TimelinePoint timelinePoint{point.startMark, point.secondMark};
        if (decision.syncEnabled && !decision.running) {
            m_pendingRaw.append(rawData[i]);
            m_pendingTimeline.append(timelinePoint);
            continue;
        }

        if (!m_pendingRaw.isEmpty()) {
            for (DWORD pendingWord : m_pendingRaw)
                acceptedRaw->append(pendingWord);
            for (const TimelinePoint& pendingPoint : m_pendingTimeline)
                timeline.append(pendingPoint);
            m_pendingRaw.clear();
            m_pendingTimeline.clear();
        }

        acceptedRaw->append(rawData[i]);
        timeline.append(timelinePoint);
    }

    return timeline;
}

QVector<TimedSample> Ltr212Worker::remapAndAttachTimeline(const QVector<double>& values,
                                                          const QVector<TimelinePoint>& timeline)
{
    QVector<TimedSample> result;
    if (values.isEmpty() || timeline.isEmpty())
        return result;

    const double sampleRate = validSampleRate(m_module->handle()->Fs);
    const quint64 timeBase = syncTimeBase(m_syncState);
    const quint64 samplesPerSecond = qMax<quint64>(1ULL, static_cast<quint64>(sampleRate + 0.5));

    result.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        const int idx = proportionalIndex(i, values.size(), timeline.size());
        const quint64 sampleIndex = m_valueSampleIndex++;

        TimedSample sample;
        sample.globalTick = sampleTick(sampleIndex, sampleRate, timeBase);
        sample.startMark = timeline[idx].startMark;
        sample.secondMark = timeline[idx].secondMark;
        sample.sampleInSecond = static_cast<quint32>(sampleIndex % samplesPerSecond);
        sample.value = values[i];
        result.append(sample);
    }

    addAcceptedValues(m_syncState, kModule212, result.size());
    return result;
}
