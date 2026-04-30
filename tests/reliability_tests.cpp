#include <QtTest>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "io/measurement_writer.h"
#include "acquisition/sync_timeline.h"

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

} // namespace

class ReliabilityTests : public QObject
{
    Q_OBJECT

private slots:
    void decodeTmarkWrap();
    void proportionalMapping();
    void syncStartFlow();
    void syncBaselineTimeout();
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
