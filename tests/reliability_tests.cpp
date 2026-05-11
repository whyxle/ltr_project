#include <QtTest>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "io/measurement_writer.h"
#include "acquisition/sync_timeline.h"
#include "acquisition/simulated_signal.h"

namespace {

DWORD tmarkWord(quint32 start, quint32 second)
{
    return ((start & 0xFFFFU) << 16) | (second & 0xFFFFU);
}

QStringList readLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    QStringList lines;
    while (!stream.atEnd())
        lines << stream.readLine();
    return lines;
}

}

class ReliabilityTests : public QObject
{
    Q_OBJECT

private slots:
    void decodeTmarkWrap();
    void proportionalMapping();
    void syncStartFlow();
    void syncBaselineTimeout();
    void simulatedSignalRatesAndTimeline();
    void simulatedSignalReactionAfterDelay();
    void measurementWriterFormat();
};

void ReliabilityTests::decodeTmarkWrap()
{
    TmarkDecodeState state;

    const TmarkPoint beforeWrap = decodeTmark(tmarkWord(65535, 65535), state);
    QCOMPARE(beforeWrap.startMark, 65535U);
    QCOMPARE(beforeWrap.secondMark, 65535U);

    const TmarkPoint afterWrap = decodeTmark(tmarkWord(0, 0), state);
    QCOMPARE(afterWrap.startMark, 65536U);
    QCOMPARE(afterWrap.secondMark, 65536U);
}

void ReliabilityTests::proportionalMapping()
{
    QCOMPARE(proportionalIndex(0, 3, 5), 0);
    QCOMPARE(proportionalIndex(1, 3, 5), 2);
    QCOMPARE(proportionalIndex(2, 3, 5), 4);
    QCOMPARE(proportionalIndex(5, 1, 10), 0);
}

void ReliabilityTests::syncStartFlow()
{
    SyncState state;
    state.needSynchronization = true;
    state.state = SyncSessionState::WaitingBaseline;
    state.sessionStartMsec = QDateTime::currentMSecsSinceEpoch();

    SyncDecision decision114 = decideSync(&state, kSyncModule114, {10, 20});
    QVERIFY(decision114.readyForStart);
    QVERIFY(!decision114.accept);

    SyncDecision decision212 = decideSync(&state, kSyncModule212, {10, 20});
    QVERIFY(decision212.readyForStart);
    QVERIFY(!decision212.accept);

    {
        QMutexLocker locker(&state.mutex);
        state.state = SyncSessionState::WaitingStartEdge;
        state.startMarkRequested = true;
        state.startMarkRequestMsec = QDateTime::currentMSecsSinceEpoch();
    }

    decision114 = decideSync(&state, kSyncModule114, {11, 20});
    QVERIFY(decision114.started);
    QVERIFY(decision114.accept);
    QVERIFY(!decision114.running);

    decision212 = decideSync(&state, kSyncModule212, {11, 20});
    QVERIFY(decision212.started);
    QVERIFY(decision212.accept);
    QVERIFY(decision212.running);

    QMutexLocker locker(&state.mutex);
    QCOMPARE(state.state, SyncSessionState::Running);
}

void ReliabilityTests::syncBaselineTimeout()
{
    SyncState state;
    state.needSynchronization = true;
    state.state = SyncSessionState::WaitingBaseline;
    state.sessionStartMsec = QDateTime::currentMSecsSinceEpoch() - 50;
    state.baselineTimeoutMs = 1;

    const QString error = checkSyncTimeout(&state, kSyncModule114);
    QVERIFY(!error.isEmpty());

    QMutexLocker locker(&state.mutex);
    QCOMPARE(state.state, SyncSessionState::Failed);
}

void ReliabilityTests::simulatedSignalRatesAndTimeline()
{
    SimulatedSignalState state114;
    SimulatedSignalConfig config114;
    config114.moduleId = kSyncModule114;
    config114.sampleRateHz = 100;
    config114.timerPeriodSec = 0.03;

    QVector<TimedSample> samples114;
    for (int i = 0; i < 100; ++i)
        samples114 += generateSimulatedSamples(state114, config114);

    QCOMPARE(samples114.size(), 300);
    QCOMPARE(samples114.at(0).globalTick, 0ULL);
    QCOMPARE(samples114.at(1).globalTick, 10000ULL);
    QCOMPARE(samples114.at(100).secondMark, 1U);
    QCOMPARE(samples114.at(100).sampleInSecond, 0U);
    QCOMPARE(samples114.last().secondMark, 2U);
    QCOMPARE(samples114.last().sampleInSecond, 99U);

    SimulatedSignalState state212;
    SimulatedSignalConfig config212;
    config212.moduleId = kSyncModule212;
    config212.sampleRateHz = 150;
    config212.timerPeriodSec = 0.03;

    QVector<TimedSample> samples212;
    for (int i = 0; i < 100; ++i)
        samples212 += generateSimulatedSamples(state212, config212);

    QCOMPARE(samples212.size(), 450);
    QCOMPARE(samples212.at(150).secondMark, 1U);
    QCOMPARE(samples212.at(150).sampleInSecond, 0U);
    QCOMPARE(samples212.last().secondMark, 2U);
    QCOMPARE(samples212.last().sampleInSecond, 149U);
}

void ReliabilityTests::simulatedSignalReactionAfterDelay()
{
    auto maxAbsInWindow = [](const QVector<TimedSample>& samples,
                             double fromSec,
                             double toSec) {
        double maxValue = 0.0;
        for (const TimedSample& sample : samples) {
            const double t = static_cast<double>(sample.globalTick) / 1000000.0;
            if (t >= fromSec && t < toSec)
                maxValue = qMax(maxValue, qAbs(sample.value));
        }
        return maxValue;
    };

    SimulatedSignalState state114;
    SimulatedSignalConfig config114;
    config114.moduleId = kSyncModule114;
    config114.sampleRateHz = 100;
    config114.timerPeriodSec = 11.0;
    const QVector<TimedSample> samples114 = generateSimulatedSamples(state114, config114);

    SimulatedSignalState state212;
    SimulatedSignalConfig config212;
    config212.moduleId = kSyncModule212;
    config212.sampleRateHz = 150;
    config212.timerPeriodSec = 11.0;
    config212.rangeVolts = 0.08;
    config212.channelCount = 4;
    config212.selectedChannel = 4;
    const QVector<TimedSample> samples212 = generateSimulatedSamples(state212, config212);

    const double baseline114 = maxAbsInWindow(samples114, 2.0, 7.5);
    const double reaction114 = maxAbsInWindow(samples114, 8.5, 10.8);
    QVERIFY2(reaction114 > baseline114 * 4.0, "LTR114 simulated reaction should be visibly larger than baseline");

    const double baseline212 = maxAbsInWindow(samples212, 2.0, 7.5);
    const double reaction212 = maxAbsInWindow(samples212, 8.5, 10.8);
    QVERIFY2(reaction212 > baseline212 * 4.0, "LTR212 simulated reaction should be visibly larger than baseline");
}

void ReliabilityTests::measurementWriterFormat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath("capture.txt");
    MeasurementWriter writer;
    QVERIFY(writer.open(path, "LTR114", 100, "mV"));

    QVector<TimedSample> samples;
    TimedSample sample;
    sample.value = 0.0000521;
    samples.append(sample);

    QVERIFY(writer.append(samples, 1000.0));
    writer.close();

    const QStringList lines = readLines(path);
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines[0], QString("100\tmV\tLTR114"));
    QCOMPARE(lines[1], QString("1\t0.0521"));
}

QTEST_MAIN(ReliabilityTests)

#include "reliability_tests.moc"
