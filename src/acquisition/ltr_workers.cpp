#include "ltr_workers.h"

#include <QThread>
#include <algorithm>
#include <utility>

#include "ltr/ltr114.h"
#include "ltr/ltr212.h"
#include "ltr/ltr_result.h"

namespace {
constexpr DWORD kRecvTimeoutMs = 50;
constexpr int kModule114 = kSyncModule114;
constexpr int kModule212 = kSyncModule212;

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
            const QString message = m_module->last_result().message.isEmpty()
                                        ? make_ltr_result(LtrApiModule::Ltr114, "LTR114_Recv", error).message
                                        : m_module->last_result().message;
            emit acquisitionError(message);
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
            emit acquisitionError(make_ltr_result(LtrApiModule::Ltr114,
                                                  "LTR114_ProcessDataTherm",
                                                  result).message);
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
            const QString message = m_module->last_result().message.isEmpty()
                                        ? make_ltr_result(LtrApiModule::Ltr212, "LTR212_Recv", error).message
                                        : m_module->last_result().message;
            emit acquisitionError(message);
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
        int processError = LTR_OK;
        const QVector<double> voltageSamples = m_module->process_data(dataToProcess, true, &processError);
        if (!m_running.load())
            break;

        if (processError != LTR_OK) {
            const QString message = m_module->last_result().message.isEmpty()
                                        ? make_ltr_result(LtrApiModule::Ltr212,
                                                          "LTR212_ProcessData",
                                                          processError).message
                                        : m_module->last_result().message;
            emit acquisitionError(message);
            break;
        }

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
