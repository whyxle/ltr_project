#ifndef LTR_WORKERS_H
#define LTR_WORKERS_H

#include <QObject>
#include <QMutex>
#include <QVector>
#include <QString>
#include <atomic>

#include "LTR/ltrapi.h"
#include "LTR/ltr114api.h"

class LTR114;
class LTR212;

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
    quint64 globalTick = 0;      // Общая шкала: 1_000_000 тиков = 1 секунда
    quint32 startMark = 0;       // Верхние 16 бит tmark
    quint32 secondMark = 0;      // Нижние 16 бит tmark
    quint32 sampleInSecond = 0;  // Позиция в текущей секунде
    double value = 0.0;
};

Q_DECLARE_METATYPE(TimedSample)
Q_DECLARE_METATYPE(QVector<TimedSample>)

class Ltr114Worker : public QObject
{
    Q_OBJECT
public:
    explicit Ltr114Worker(LTR114* module, SyncState* syncState, QObject* parent = nullptr);

public slots:
    void run();
    void stopAcquisition();

signals:
    void newVoltageSamples(const QVector<TimedSample>& voltageSamples);
    void acquisitionError(const QString& message);
    void syncReadyForStart(int moduleId, quint32 baselineStart, quint32 baselineSecond);
    void syncStarted(int moduleId, quint32 startMark, quint32 secondMark, quint64 droppedBeforeStart);
    void finished();

private:
    QVector<TimelinePoint> buildTimeline(const QVector<DWORD>& rawData,
                                         const QVector<DWORD>& tmarks,
                                         QVector<DWORD>* acceptedRaw);
    QVector<TimedSample> attachTimeline(const QVector<double>& values, const QVector<TimelinePoint>& timeline);

    LTR114* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{true};
    TmarkDecodeState m_tmarkState;
    quint64 m_valueSampleIndex = 0;
    bool m_readySignalEmitted = false;
    bool m_startedSignalEmitted = false;
    QVector<DWORD> m_pendingRaw;
    QVector<TimelinePoint> m_pendingTimeline;
};

class Ltr212Worker : public QObject
{
    Q_OBJECT
public:
    explicit Ltr212Worker(LTR212* module, SyncState* syncState, QObject* parent = nullptr);

public slots:
    void run();
    void stopAcquisition();

signals:
    void newVoltageSamples(const QVector<TimedSample>& voltageSamples);
    void acquisitionError(const QString& message);
    void syncReadyForStart(int moduleId, quint32 baselineStart, quint32 baselineSecond);
    void syncStarted(int moduleId, quint32 startMark, quint32 secondMark, quint64 droppedBeforeStart);
    void finished();

private:
    QVector<TimelinePoint> buildTimeline(const QVector<DWORD>& rawData,
                                         const QVector<DWORD>& tmarks,
                                         QVector<DWORD>* acceptedRaw);
    QVector<TimedSample> remapAndAttachTimeline(const QVector<double>& values, const QVector<TimelinePoint>& timeline);

    LTR212* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{true};
    TmarkDecodeState m_tmarkState;
    quint64 m_valueSampleIndex = 0;
    bool m_readySignalEmitted = false;
    bool m_startedSignalEmitted = false;
    QVector<DWORD> m_pendingRaw;
    QVector<TimelinePoint> m_pendingTimeline;
};


#endif // LTR_WORKERS_H
