#include "sync_timeline.h"

#include <QDateTime>
#include <QMutexLocker>

namespace {

quint32 unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch)
{
    if (rawValue < lastRaw && static_cast<quint16>(lastRaw - rawValue) > 32768U)
        epoch += 65536U;

    lastRaw = rawValue;
    return epoch + static_cast<quint32>(rawValue);
}

SyncModuleState& moduleState(SyncState& state, int moduleId)
{
    return moduleId == kSyncModule212 ? state.ltr212 : state.ltr114;
}

} // namespace

quint32 extractStartMark(DWORD tmarkWord)
{
    return (tmarkWord >> 16) & 0xFFFF;
}

quint32 extractSecondMark(DWORD tmarkWord)
{
    return tmarkWord & 0xFFFF;
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
    SyncModuleState& other = moduleState(*syncState, moduleId == kSyncModule212 ? kSyncModule114 : kSyncModule212);

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
                                        .arg(syncModuleName(moduleId))
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
                                                .arg(syncModuleName(moduleId))
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
                                        .arg(syncModuleName(moduleId))
                                        .arg(syncState->baselineTimeoutMs);
        return syncState->failureMessage;
    }

    if (syncState->state == SyncSessionState::WaitingStartEdge
        && !current.startSeen
        && syncState->startMarkRequestMsec > 0
        && nowMsec - syncState->startMarkRequestMsec > syncState->startTimeoutMs) {
        syncState->state = SyncSessionState::Failed;
        syncState->failureMessage = QString("%1: START mark was not received within %2 ms")
                                        .arg(syncModuleName(moduleId))
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

QString syncModuleName(int moduleId)
{
    return moduleId == kSyncModule212 ? QStringLiteral("LTR212") : QStringLiteral("LTR114");
}
