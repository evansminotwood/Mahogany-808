#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    String msString (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; }
    String pctString (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; }
}

AudioProcessorValueTreeState::ParameterLayout Forge808Processor::createLayout()
{
    using P = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto pct = Attr().withStringFromValueFunction (pctString);
    auto ms  = Attr().withStringFromValueFunction (msString);

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::tune, 1 }, "Tune", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
                                       Attr().withLabel ("st")));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::fine, 1 }, "Fine", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                       Attr().withLabel ("ct")));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::shape, 1 }, "Shape", NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::sub, 1 }, "Sub", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::pitchAmt, 1 }, "Punch", NormalisableRange<float> (0.0f, 48.0f, 0.1f), 14.0f,
                                       Attr().withLabel ("st")));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::pitchDecay, 1 }, "Punch Decay", skewed (2.0f, 500.0f, 60.0f), 45.0f, ms));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::click, 1 }, "Click", NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::clickTone, 1 }, "Click Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::attack, 1 }, "Attack", skewed (0.0f, 200.0f, 15.0f), 1.0f, ms));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::decay, 1 }, "Decay", skewed (20.0f, 8000.0f, 1200.0f), 1400.0f, ms));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::sustain, 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::release, 1 }, "Release", skewed (5.0f, 4000.0f, 400.0f), 250.0f, ms));

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::driveMode, 1 }, "Drive Mode",
                                                          StringArray { "Soft", "Hard", "Tube", "Fold", "Crush" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::driveMix, 1 }, "Drive Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::tone, 1 }, "Tone", skewed (100.0f, 20000.0f, 2000.0f), 9000.0f,
                                       Attr().withStringFromValueFunction ([] (float v, int)
                                       { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; })));

    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::glide, 1 }, "Glide", skewed (0.0f, 1000.0f, 120.0f), 90.0f, ms));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::glideMode, 1 }, "Glide Mode",
                                                          StringArray { "Off", "Legato", "Always" }, 1));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::bendRange, 1 }, "Bend Range", NormalisableRange<float> (1.0f, 24.0f, 1.0f), 12.0f,
                                       Attr().withLabel ("st")));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -3.0f,
                                       Attr().withLabel ("dB")));

    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::renderNote, 1 }, "Render Note", 12, 72, 36,
                                                       AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                       { return MidiMessage::getMidiNoteName (v, true, true, 5); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamIDs::renderLength, 1 }, "Render Length", skewed (0.1f, 8.0f, 1.5f), 1.5f,
                                       Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::normalize, 1 }, "Normalize Export", true));

    return { ps.begin(), ps.end() };
}

//==============================================================================
const std::vector<Preset808>& Forge808Processor::factoryPresets()
{
    using namespace ParamIDs;
    static const std::vector<Preset808> presets {
        { "Classic 808",   { { shape, 0.0f },  { sub, 0.0f },  { pitchAmt, 10.0f }, { pitchDecay, 40.0f },  { click, 0.2f },  { clickTone, 0.4f },
                             { attack, 1.0f }, { decay, 2200.0f }, { sustain, 0.0f },  { release, 300.0f }, { drive, 0.05f }, { driveMode, 0 },
                             { driveMix, 1.0f }, { tone, 12000.0f }, { glide, 0.0f }, { glideMode, 0 }, { output, -3.0f } } },
        { "Trap Slide",    { { shape, 0.2f },  { sub, 0.1f },  { pitchAmt, 14.0f }, { pitchDecay, 50.0f },  { click, 0.25f }, { clickTone, 0.5f },
                             { attack, 1.0f }, { decay, 1500.0f }, { sustain, 0.45f }, { release, 250.0f }, { drive, 0.3f },  { driveMode, 0 },
                             { driveMix, 1.0f }, { tone, 9000.0f }, { glide, 110.0f }, { glideMode, 1 }, { output, -3.0f } } },
        { "Distorted Boom",{ { shape, 0.35f }, { sub, 0.2f },  { pitchAmt, 18.0f }, { pitchDecay, 55.0f },  { click, 0.3f },  { clickTone, 0.6f },
                             { attack, 1.0f }, { decay, 1800.0f }, { sustain, 0.5f },  { release, 300.0f }, { drive, 0.7f },  { driveMode, 2 },
                             { driveMix, 0.9f }, { tone, 6500.0f }, { glide, 90.0f }, { glideMode, 1 }, { output, -6.0f } } },
        { "Punchy Short",  { { shape, 0.25f }, { sub, 0.0f },  { pitchAmt, 24.0f }, { pitchDecay, 25.0f },  { click, 0.5f },  { clickTone, 0.7f },
                             { attack, 0.5f }, { decay, 450.0f },  { sustain, 0.0f },  { release, 120.0f }, { drive, 0.4f },  { driveMode, 0 },
                             { driveMix, 1.0f }, { tone, 10000.0f }, { glide, 0.0f }, { glideMode, 0 }, { output, -3.0f } } },
        { "Long Sub",      { { shape, 0.0f },  { sub, 0.3f },  { pitchAmt, 5.0f },  { pitchDecay, 80.0f },  { click, 0.08f }, { clickTone, 0.2f },
                             { attack, 3.0f }, { decay, 5000.0f }, { sustain, 0.8f },  { release, 600.0f }, { drive, 0.1f },  { driveMode, 0 },
                             { driveMix, 1.0f }, { tone, 4000.0f }, { glide, 150.0f }, { glideMode, 1 }, { output, -3.0f } } },
        { "Hard Clip Knock",{ { shape, 0.5f }, { sub, 0.0f },  { pitchAmt, 20.0f }, { pitchDecay, 35.0f },  { click, 0.4f },  { clickTone, 0.8f },
                             { attack, 0.5f }, { decay, 1200.0f }, { sustain, 0.3f },  { release, 200.0f }, { drive, 0.6f },  { driveMode, 1 },
                             { driveMix, 1.0f }, { tone, 8000.0f }, { glide, 70.0f }, { glideMode, 1 }, { output, -6.0f } } },
        { "Folded Growl",  { { shape, 0.3f },  { sub, 0.25f }, { pitchAmt, 12.0f }, { pitchDecay, 60.0f },  { click, 0.2f },  { clickTone, 0.5f },
                             { attack, 2.0f }, { decay, 2000.0f }, { sustain, 0.55f }, { release, 300.0f }, { drive, 0.55f }, { driveMode, 3 },
                             { driveMix, 0.8f }, { tone, 5000.0f }, { glide, 100.0f }, { glideMode, 1 }, { output, -6.0f } } },
        { "Lo-Fi Crush",   { { shape, 0.2f },  { sub, 0.1f },  { pitchAmt, 12.0f }, { pitchDecay, 45.0f },  { click, 0.3f },  { clickTone, 0.3f },
                             { attack, 1.0f }, { decay, 1600.0f }, { sustain, 0.4f },  { release, 250.0f }, { drive, 0.45f }, { driveMode, 4 },
                             { driveMix, 0.7f }, { tone, 3500.0f }, { glide, 80.0f }, { glideMode, 1 }, { output, -4.0f } } },
    };
    return presets;
}

int Forge808Processor::getNumPrograms() { return (int) factoryPresets().size(); }

const String Forge808Processor::getProgramName (int index)
{
    return isPositiveAndBelow (index, getNumPrograms()) ? String (factoryPresets()[(size_t) index].name) : String();
}

void Forge808Processor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    userPresetDefaults.clear();
    for (const auto& [id, value] : factoryPresets()[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void Forge808Processor::captureUserPresetDefaults()
{
    userPresetDefaults.clear();
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<RangedAudioParameter*> (param))
            userPresetDefaults[p->getParameterID()] = p->getValue();
}

void Forge808Processor::resetToPresetDefault (const String& paramID)
{
    auto* p = apvts.getParameter (paramID);
    if (p == nullptr)
        return;

    auto value = p->getDefaultValue();
    if (const auto it = userPresetDefaults.find (paramID); it != userPresetDefaults.end())
        value = it->second;
    else
        for (const auto& [id, v] : factoryPresets()[(size_t) currentProgram].values)
            if (paramID == id)
                value = p->convertTo0to1 (v);

    p->beginChangeGesture();
    p->setValueNotifyingHost (value);
    p->endChangeGesture();
}

//==============================================================================
Forge808Processor::Forge808Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Forge808", createLayout())
{
}

bool Forge808Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

Params808 Forge808Processor::snapshotParams() const
{
    Params808 p;
    p.tune       = get (ParamIDs::tune);
    p.fine       = get (ParamIDs::fine);
    p.shape      = get (ParamIDs::shape);
    p.sub        = get (ParamIDs::sub);
    p.pitchAmt   = get (ParamIDs::pitchAmt);
    p.pitchDecay = get (ParamIDs::pitchDecay);
    p.click      = get (ParamIDs::click);
    p.clickTone  = get (ParamIDs::clickTone);
    p.attack     = get (ParamIDs::attack);
    p.decay      = get (ParamIDs::decay);
    p.sustain    = get (ParamIDs::sustain);
    p.release    = get (ParamIDs::release);
    p.drive      = get (ParamIDs::drive);
    p.driveMode  = (int) get (ParamIDs::driveMode);
    p.driveMix   = get (ParamIDs::driveMix);
    p.tone       = get (ParamIDs::tone);
    p.glide      = get (ParamIDs::glide);
    p.glideMode  = (int) get (ParamIDs::glideMode);
    p.bendRange  = get (ParamIDs::bendRange);
    p.output     = get (ParamIDs::output);
    return p;
}

void Forge808Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (snapshotParams());
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void Forge808Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    // On-screen keyboard notes get merged into the host MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    engine.setParams (snapshotParams());
    engine.process (buffer, midi);
}

//==============================================================================
AudioBuffer<float> Forge808Processor::renderPreview (double sampleRate) const
{
    return OfflineRender808::render (snapshotParams(), (int) get (ParamIDs::renderNote),
                                     get (ParamIDs::renderLength), sampleRate);
}

String Forge808Processor::suggestedFileName() const
{
    const auto note = MidiMessage::getMidiNoteName ((int) get (ParamIDs::renderNote), true, true, 5);
    auto preset = String (factoryPresets()[(size_t) currentProgram].name).removeCharacters (" ");
    return "808_" + preset + "_" + note.replace ("#", "s") + ".wav";
}

String Forge808Processor::exportWav (const File& file, double sampleRate) const
{
    auto buffer = renderPreview (sampleRate);

    if (get (ParamIDs::normalize) > 0.5f)
    {
        const auto peak = buffer.getMagnitude (0, buffer.getNumSamples());
        if (peak > 0.0f)
            buffer.applyGain (Decibels::decibelsToGain (-0.3f) / peak);
    }

    // Short fade-out so the file never ends on a click.
    const int fade = jmin (buffer.getNumSamples(), (int) (sampleRate * 0.005));
    buffer.applyGainRamp (buffer.getNumSamples() - fade, fade, 1.0f, 0.0f);

    file.deleteFile();
    std::unique_ptr<OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return "Couldn't write to " + file.getFullPathName();

    WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                                          .withNumChannels (1)
                                                                          .withBitsPerSample (24));
    if (writer == nullptr)
        return "Couldn't create a WAV writer";

    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return {};
}

//==============================================================================
void Forge808Processor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Forge808Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentProgram = jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("program", 0));
            apvts.replaceState (state);
        }
}

AudioProcessorEditor* Forge808Processor::createEditor() { return new Forge808Editor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Forge808Processor(); }
