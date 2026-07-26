#include "InputSidebarDSP.h"

namespace invis::modules {

void InputSidebarDSP::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& paramPrefix)
{
    // 1. Input Trim (-24 dB .. +24 dB, default 0.0 dB)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPrefix + "trim", 1 },
        "Input Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // 2. HPF High-Pass Filter (20 Hz .. 2000 Hz, default 20 Hz OFF)
    juce::NormalisableRange<float> hpfRange(20.0f, 2000.0f, 1.0f, 0.4f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPrefix + "hpf_freq", 1 },
        "Input HPF",
        hpfRange,
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));

    // 2b. HPF Slope
    juce::StringArray slopeNames;
    for (int i = 0; i < kNumFilterSlopes; ++i)
        slopeNames.add(getFilterSlopeName(static_cast<FilterSlope>(i)));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramPrefix + "hpf_slope", 1 },
        "Input HPF Slope",
        slopeNames,
        static_cast<int>(FilterSlope::Slope12)
    ));

    // 3. LPF Low-Pass Filter (1000 Hz .. 20000 Hz, default 20000 Hz OFF)
    juce::NormalisableRange<float> lpfRange(1000.0f, 20000.0f, 1.0f, 0.4f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPrefix + "lpf_freq", 1 },
        "Input LPF",
        lpfRange,
        20000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));

    // 3b. LPF Slope
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { paramPrefix + "lpf_slope", 1 },
        "Input LPF Slope",
        slopeNames,
        static_cast<int>(FilterSlope::Slope12)
    ));
}

void InputSidebarDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);

    hpfFilter.prepare(spec);
    lpfFilter.prepare(spec);
    analyser.prepare(spec);
    reset();
}

void InputSidebarDSP::reset()
{
    hpfFilter.reset();
    lpfFilter.reset();
    analyser.reset();

    preTap.reset();
    postHpfTap.reset();
    postLpfTap.reset();

    hpfLampLevel.store(0.0f, std::memory_order_relaxed);
    lpfLampLevel.store(0.0f, std::memory_order_relaxed);

    peakL.store(0.0f, std::memory_order_relaxed);
    peakR.store(0.0f, std::memory_order_relaxed);
}

double InputSidebarDSP::computeMeanSquare(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return 0.0;

    double sum = 0.0;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }

    return sum / static_cast<double>(numSamples * numCh);
}

void InputSidebarDSP::processBlock(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix, bool bypassed)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    // 1. Fetch APVTS parameters
    const float trimDb = apvts.getRawParameterValue(paramPrefix + "trim")->load();
    const float hpfFreq = apvts.getRawParameterValue(paramPrefix + "hpf_freq")->load();
    const float lpfFreq = apvts.getRawParameterValue(paramPrefix + "lpf_freq")->load();

    const auto hpfSlope = static_cast<FilterSlope>(
        juce::roundToInt(apvts.getRawParameterValue(paramPrefix + "hpf_slope")->load()));
    const auto lpfSlope = static_cast<FilterSlope>(
        juce::roundToInt(apvts.getRawParameterValue(paramPrefix + "lpf_slope")->load()));

    // 2. Apply Input Trim Gain
    const float gainLinear = juce::Decibels::decibelsToGain(trimDb);
    if (!bypassed && std::abs(gainLinear - 1.0f) > 0.001f)
    {
        buffer.applyGain(gainLinear);
    }

    const double blockSeconds = static_cast<double>(numSamples) / sampleRate;

    // 2b. Energy tap BEFORE the filters - the reference the HPF stage is measured against
    preTap.push(computeMeanSquare(buffer), blockSeconds);
    const bool signalPresent = (preTap.getDb() > kEnergyGateDb);

    // 3. HPF Filter Update & Processing (Bypassed if <= 20.5 Hz)
    const bool isHpfOff = bypassed || (hpfFreq <= 20.5f);
    if (!isHpfOff)
    {
        hpfFilter.update(hpfFreq, hpfSlope, true);
        hpfFilter.process(buffer);
    }

    // 3b. Energy removed by the HPF stage alone
    postHpfTap.push(computeMeanSquare(buffer), blockSeconds);

    hpfEngaged.store(!isHpfOff, std::memory_order_relaxed);
    if (isHpfOff || !signalPresent)
    {
        // Nothing measurable: release toward dark rather than reporting a ratio of noise
        hpfLampLevel.store(0.0f, std::memory_order_relaxed);
    }
    else
    {
        const float removedDb = juce::jlimit(0.0f, kMaxRemovedDb, preTap.getDb() - postHpfTap.getDb());
        hpfLampLevel.store(removedDb / kMaxRemovedDb, std::memory_order_relaxed);
    }

    // 4. LPF Filter Update & Processing (Bypassed if >= 19950 Hz)
    const bool isLpfOff = bypassed || (lpfFreq >= 19950.0f);
    if (!isLpfOff)
    {
        lpfFilter.update(lpfFreq, lpfSlope, false);
        lpfFilter.process(buffer);
    }

    // 4b. Energy removed by the LPF stage alone - referenced to the POST-HPF signal
    postLpfTap.push(computeMeanSquare(buffer), blockSeconds);

    lpfEngaged.store(!isLpfOff, std::memory_order_relaxed);
    if (isLpfOff || !signalPresent)
    {
        lpfLampLevel.store(0.0f, std::memory_order_relaxed);
    }
    else
    {
        const float removedDb = juce::jlimit(0.0f, kMaxRemovedDb, postHpfTap.getDb() - postLpfTap.getDb());
        lpfLampLevel.store(removedDb / kMaxRemovedDb, std::memory_order_relaxed);
    }

    // 5. Measure Output Peak Levels for Input Sidebar Metering
    const float magL = (buffer.getNumChannels() > 0) ? buffer.getMagnitude(0, 0, numSamples) : 0.0f;
    const float magR = (buffer.getNumChannels() > 1) ? buffer.getMagnitude(1, 0, numSamples) : magL;

    peakL.store(magL, std::memory_order_relaxed);
    peakR.store(magR, std::memory_order_relaxed);

    // 6. Numeric readouts (RMS / PEAK / LUFS-S) for the meter
    analyser.process(buffer);
}

} // namespace invis::modules
