#include "simulated_signal.h"

#include <QtGlobal>

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr quint64 kTicksPerSecond = 1000000ULL;

double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge1 <= edge0)
        return value >= edge1 ? 1.0 : 0.0;

    const double x = clamp01((value - edge0) / (edge1 - edge0));
    return x * x * (3.0 - 2.0 * x);
}

double decayingPositiveBump(double t, double frequency, double damping)
{
    if (t <= 0.0)
        return 0.0;

    const double onset = smoothStep(0.0, 0.45, t);
    const double pulse = 0.5 - 0.5 * std::cos(2.0 * kPi * frequency * t);
    return onset * std::exp(-damping * t) * pulse;
}

double rangeGain(double rangeVolts, bool ltr212)
{
    const double baseRange = ltr212 ? 0.08 : 0.4;
    const double normalizedRange = qBound(0.2, rangeVolts / baseRange, 25.0);
    return std::sqrt(normalizedRange);
}

double simulatedValue(const SimulatedSignalConfig& config, double t)
{
    const bool ltr212 = config.moduleId == kSyncModule212;
    const double phase = ltr212 ? 1.35 : 0.45;
    const int channel = qMax(1, config.selectedChannel);
    const int channelCount = qMax(1, config.channelCount);
    const double gain = rangeGain(qMax(0.001, config.rangeVolts), ltr212);
    const double channelGain = 1.0 + 0.035 * static_cast<double>(channel - 1);
    const double countGain = ltr212 ? 1.0 + 0.045 * static_cast<double>(channelCount - 1) : 1.0;
    const double referenceGain = ltr212 && config.referenceVoltage < 3.0 ? 0.86 : 1.0;

    const double baseAmp = (ltr212 ? 0.00042 : 0.00075) * gain * channelGain * referenceGain;
    const double responseAmp = (ltr212 ? 0.0065 : 0.018) * gain * channelGain * countGain * referenceGain;

    const double drift = 0.42 * baseAmp * std::sin(2.0 * kPi * 0.055 * t + phase);
    const double carrier = 0.22 * baseAmp * std::sin(2.0 * kPi * (ltr212 ? 1.7 : 1.1) * t + phase * 0.7);
    const double sensorNoise = baseAmp
                               * (0.16 * std::sin(2.0 * kPi * (ltr212 ? 9.0 : 6.0) * t + phase)
                                  + 0.08 * std::sin(2.0 * kPi * (ltr212 ? 17.0 : 11.0) * t + phase * 2.1));

    const double afterInteraction = t - qMax(0.0, config.interactionDelaySec);
    double response = 0.0;
    if (afterInteraction > 0.0) {
        const double ramp = smoothStep(0.0, 2.2, afterInteraction);
        const double plateau = responseAmp * ramp;
        const double firstTouch = responseAmp
                                  * (ltr212 ? 0.95 : 0.78)
                                  * decayingPositiveBump(afterInteraction,
                                                         ltr212 ? 1.45 : 1.1,
                                                         ltr212 ? 0.82 : 0.62);
        const double ringing = responseAmp
                               * (ltr212 ? 0.13 : 0.10)
                               * std::exp(-0.55 * afterInteraction)
                               * std::sin(2.0 * kPi * (ltr212 ? 2.7 : 2.15) * afterInteraction + phase);

        response = plateau + firstTouch + ringing;
        if (config.acCoupled)
            response = response * 0.58 + firstTouch * 0.38;
    }

    double offset = config.unipolarRange ? qMax(0.0004, config.rangeVolts * 0.12) : 0.0;
    if (config.acCoupled)
        offset = 0.0;

    double value = offset + drift + carrier + sensorNoise + response;

    const double rangeLimit = qMax(0.001, config.rangeVolts) * 0.95;
    if (config.unipolarRange)
        value = qBound(0.0, value, rangeLimit);
    else
        value = qBound(-rangeLimit, value, rangeLimit);

    return value;
}

}

QVector<TimedSample> generateSimulatedSamples(SimulatedSignalState& state,
                                              const SimulatedSignalConfig& config)
{
    QVector<TimedSample> samples;

    const int sampleRateHz = qMax(1, config.sampleRateHz);
    const double sampleRate = static_cast<double>(sampleRateHz);
    state.accumulator += sampleRate * qMax(0.0, config.timerPeriodSec);

    const int samplesToGenerate = static_cast<int>(state.accumulator);
    state.accumulator -= static_cast<double>(samplesToGenerate);

    if (samplesToGenerate <= 0)
        return samples;

    samples.reserve(samplesToGenerate);
    for (int i = 0; i < samplesToGenerate; ++i) {
        const quint64 sampleIndex = state.sampleIndex++;
        const double t = static_cast<double>(sampleIndex) / sampleRate;

        TimedSample sample;
        sample.globalTick = sampleTick(sampleIndex, sampleRate, kTicksPerSecond);
        sample.startMark = 0;
        sample.secondMark = static_cast<quint32>(sampleIndex / static_cast<quint64>(sampleRateHz));
        sample.sampleInSecond = static_cast<quint32>(sampleIndex % static_cast<quint64>(sampleRateHz));
        sample.value = simulatedValue(config, t);
        samples.append(sample);
    }

    return samples;
}
