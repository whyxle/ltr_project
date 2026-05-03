#ifndef SYNC_TIMELINE_H
#define SYNC_TIMELINE_H

#include <QMutex>
#include <QMetaType>
#include <QString>
#include <QVector>

#include "LTR/ltrapi.h"

constexpr int kSyncModule114 = 0;
constexpr int kSyncModule212 = 1;

enum class SyncSessionState
{
    Disabled,
    WaitingBaseline,
    WaitingStartEdge,
    Running,
    Failed
};

struct SyncModuleState
{
    bool baselineSeen = false;
    bool startSeen = false;
    quint32 baselineStart = 0;
    quint32 baselineSecond = 0;
    quint32 startMark = 0;
    quint32 startSecond = 0;
    quint64 droppedBeforeStart = 0;
    quint64 acceptedRaw = 0;
    quint64 acceptedValues = 0;
};

struct SyncState
{
    QMutex mutex;
    bool needSynchronization = false;
    SyncSessionState state = SyncSessionState::Disabled;
    qint64 sessionStartMsec = 0;
    bool startMarkRequested = false;
    qint64 startMarkRequestMsec = 0;
    int baselineTimeoutMs = 5000;
    int startTimeoutMs = 5000;
    QString failureMessage;
    SyncModuleState ltr114;
    SyncModuleState ltr212;
    quint64 timeBaseTicks = 1000000ULL;
};

struct TmarkDecodeState
{
    quint16 lastStartRaw = 0;
    quint16 lastSecondRaw = 0;
    quint32 startEpoch = 0;
    quint32 secondEpoch = 0;
    bool initialized = false;
};

struct TmarkPoint
{
    quint32 startMark = 0;
    quint32 secondMark = 0;
};

struct TimelinePoint
{
    quint32 startMark = 0;
    quint32 secondMark = 0;
};

struct TimedSample
{
    quint64 globalTick = 0;
    quint32 startMark = 0;
    quint32 secondMark = 0;
    quint32 sampleInSecond = 0;
    double value = 0.0;
};

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

Q_DECLARE_METATYPE(TimedSample);
Q_DECLARE_METATYPE(QVector<TimedSample>);

quint32 extractStartMark(DWORD tmarkWord);
quint32 extractSecondMark(DWORD tmarkWord);
TmarkPoint decodeTmark(DWORD tmarkWord, TmarkDecodeState& state);
double validSampleRate(double sampleRate);
int proportionalIndex(int valueIndex, int valueCount, int timelineCount);
quint64 sampleTick(quint64 sampleIndex, double sampleRate, quint64 timeBaseTicks);
SyncDecision decideSync(SyncState* syncState, int moduleId, const TmarkPoint& point);
QString checkSyncTimeout(SyncState* syncState, int moduleId);
void addAcceptedValues(SyncState* syncState, int moduleId, qsizetype count);
quint64 syncTimeBase(SyncState* syncState);
QString syncModuleName(int moduleId);

#endif
