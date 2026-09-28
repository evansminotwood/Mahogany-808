#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine808.h"

namespace ParamIDs
{
    inline constexpr auto tune = "tune", fine = "fine", shape = "shape", sub = "sub",
        pitchAmt = "pitchAmt", pitchDecay = "pitchDecay", click = "click", clickTone = "clickTone",
        attack = "attack", decay = "decay", sustain = "sustain", release = "release",
        drive = "drive", driveMode = "driveMode", driveMix = "driveMix", tone = "tone",
        glide = "glide", glideMode = "glideMode", bendRange = "bendRange", output = "output",
        renderNote = "renderNote", renderLength = "renderLength", normalize = "normalize";
}

struct Preset808
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

class Forge808Processor final : public juce::AudioProcessor
{
public:
    Forge808Processor();
    ~Forge808Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mahogany 808"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Current patch as a plain struct (message or audio thread).
    Params808 snapshotParams() const;

    // Renders one note of the current patch using the "Render" settings.
    juce::AudioBuffer<float> renderPreview (double sampleRate) const;
    // Writes the rendered note as a 24-bit WAV. Returns an error message, or empty on success.
    juce::String exportWav (const juce::File& file, double sampleRate = 44100.0) const;
    juce::String suggestedFileName() const;

    static const std::vector<Preset808>& factoryPresets();

    // Makes the current parameter values the "preset defaults" (after loading/saving a user preset).
    void captureUserPresetDefaults();
    // Resets one parameter to the current preset's value (right-click on a control).
    void resetToPresetDefault (const juce::String& paramID);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    Engine808 engine;
    int currentProgram = 0;
    std::map<juce::String, float> userPresetDefaults; // normalised; empty while a factory preset is active

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Forge808Processor)
};
