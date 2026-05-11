#ifndef SIMULATED_SIGNAL_H
#define SIMULATED_SIGNAL_H

#include <QVector>

#include "acquisition/sync_timeline.h"

struct SimulatedSignalConfig
{
    int moduleId = kSyncModule114;
    int sampleRateHz = 100;
    double timerPeriodSec = 0.03;
    int selectedChannel = 1;
    int channelCount = 1;
    double rangeVolts = 0.4;
    bool unipolarRange = false;
    bool acCoupled = false;
    double referenceVoltage = 5.0;
    double interactionDelaySec = 8.0;
};

struct SimulatedSignalState
{
    double accumulator = 0.0;
    quint64 sampleIndex = 0;

    void reset()
    {
        accumulator = 0.0;
        sampleIndex = 0;
    }
};

QVector<TimedSample> generateSimulatedSamples(SimulatedSignalState& state,
                                              const SimulatedSignalConfig& config);

#endif
