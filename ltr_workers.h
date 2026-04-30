#ifndef LTR_WORKERS_H
#define LTR_WORKERS_H

#include <QObject>
#include <QVector>
#include <QString>
#include <atomic>

#include "LTR/ltrapi.h"
#include "LTR/ltr114api.h"
#include "sync_timeline.h"

class LTR114;
class LTR212;

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
