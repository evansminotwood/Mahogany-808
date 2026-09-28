// Headless checks for the 808 engine plus a UI snapshot.
// Usage: Forge808Tests [outputDir]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! ok) ++failures;
}

static bool allFinite (const AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

// Rough pitch estimate from positive-going zero crossings over [start, start+len).
static double estimateHz (const AudioBuffer<float>& b, double sr, int start, int len)
{
    int crossings = 0, first = -1, last = -1;
    const auto* d = b.getReadPointer (0);
    for (int i = start + 1; i < start + len; ++i)
        if (d[i - 1] <= 0.0f && d[i] > 0.0f)
        {
            if (first < 0) first = i;
            last = i;
            ++crossings;
        }
    return crossings > 1 ? (crossings - 1) * sr / (last - first) : 0.0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juceInit;
    const File outDir = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                 : File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    Forge808Processor proc;
    const double sr = 48000.0;
    proc.setPlayConfigDetails (0, 2, sr, 256);
    proc.prepareToPlay (sr, 256);

    std::cout << "Realtime render\n";
    {
        // Pure sine, no punch, no drive: pitch should land on the note.
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (ParamIDs::shape, 0.0f); set (ParamIDs::pitchAmt, 0.0f); set (ParamIDs::drive, 0.0f);
        set (ParamIDs::click, 0.0f); set (ParamIDs::sustain, 1.0f); set (ParamIDs::glideMode, 0);
        set (ParamIDs::tone, 20000.0f);

        AudioBuffer<float> out (2, (int) sr);
        AudioBuffer<float> block (2, 256);
        int pos = 0;
        while (pos < out.getNumSamples())
        {
            const int n = jmin (256, out.getNumSamples() - pos);
            block.setSize (2, n, false, false, true);
            MidiBuffer midi;
            if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, 33, (uint8) 127), 10); // A1 = 55 Hz
            proc.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
            pos += n;
        }
        check (allFinite (out), "output is finite");
        check (out.getMagnitude (0, out.getNumSamples()) > 0.1f, "note produces sound");
        const auto hz = estimateHz (out, sr, (int) (sr * 0.2), (int) (sr * 0.6));
        std::cout << "        measured " << hz << " Hz\n";
        check (std::abs (hz - 55.0) < 0.5, "A1 plays at 55 Hz");

        // Glide: hold A1, then press A2 in legato mode; pitch should end at 110 Hz.
        set (ParamIDs::glideMode, 1); set (ParamIDs::glide, 100.0f);
        proc.reset();
        proc.prepareToPlay (sr, 256);
        pos = 0;
        while (pos < out.getNumSamples())
        {
            const int n = jmin (256, out.getNumSamples() - pos);
            block.setSize (2, n, false, false, true);
            MidiBuffer midi;
            if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, 33, (uint8) 127), 0);
            if (pos == 256 * 40) midi.addEvent (MidiMessage::noteOn (1, 45, (uint8) 127), 0);
            proc.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
            pos += n;
        }
        const auto hz2 = estimateHz (out, sr, (int) (sr * 0.5), (int) (sr * 0.45));
        std::cout << "        after glide " << hz2 << " Hz\n";
        check (std::abs (hz2 - 110.0) < 1.0, "legato glide reaches A2 (110 Hz)");
    }

    std::cout << "Presets and export\n";
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto wav = outDir.getChildFile (proc.suggestedFileName());
        const auto err = proc.exportWav (wav, 44100.0);
        check (err.isEmpty() && wav.getSize() > 1000, "export " + proc.getProgramName (i) + " -> " + wav.getFileName());

        AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (wav));
        check (reader != nullptr && reader->bitsPerSample == 24 && reader->sampleRate == 44100.0,
               "  24-bit / 44.1 kHz, " + String (reader ? reader->lengthInSamples / 44100.0 : 0.0, 2) + " s");
        auto buf = proc.renderPreview (44100.0);
        check (allFinite (buf) && buf.getMagnitude (0, buf.getNumSamples()) > 0.05f, "  preview is finite and audible");
    }

    std::cout << "State round-trip\n";
    {
        proc.setCurrentProgram (2);
        MemoryBlock state;
        proc.getStateInformation (state);
        Forge808Processor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        bool same = other.getCurrentProgram() == 2;
        for (auto* param : proc.getParameters())
        {
            auto* ranged = dynamic_cast<RangedAudioParameter*> (param);
            const auto va = proc.apvts.getRawParameterValue (ranged->getParameterID())->load();
            const auto vb = other.apvts.getRawParameterValue (ranged->getParameterID())->load();
            if (std::abs (va - vb) > 1.0e-3f * jmax (1.0f, std::abs (va)))
            {
                std::cout << "        mismatch " << ranged->getParameterID() << " " << va << " vs " << vb << "\n";
                same = false;
            }
        }
        check (same, "state restores params and preset");
    }

    std::cout << "Reset to preset default\n";
    {
        auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        auto value = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

        proc.setCurrentProgram (1); // Trap Slide: shape 0.2, doesn't set tune
        set (ParamIDs::shape, 0.9f); set (ParamIDs::tune, 5.0f); set (ParamIDs::driveMode, 3);
        proc.resetToPresetDefault (ParamIDs::shape);
        proc.resetToPresetDefault (ParamIDs::tune);
        proc.resetToPresetDefault (ParamIDs::driveMode);
        check (std::abs (value (ParamIDs::shape) - 0.2f) < 1.0e-3f, "knob resets to factory preset value");
        check (value (ParamIDs::tune) == 0.0f, "param not in preset resets to its default");
        check (value (ParamIDs::driveMode) == 0.0f, "choice resets to factory preset value");

        // Loading a user preset makes its values the defaults.
        set (ParamIDs::shape, 0.6f);
        auto userState = proc.apvts.copyState().createCopy();
        set (ParamIDs::shape, 0.1f);
        proc.apvts.replaceState (userState);
        proc.captureUserPresetDefaults();
        set (ParamIDs::shape, 0.95f);
        proc.resetToPresetDefault (ParamIDs::shape);
        check (std::abs (value (ParamIDs::shape) - 0.6f) < 1.0e-3f, "knob resets to loaded user preset value");
    }

    std::cout << "Editor snapshot\n";
    {
        proc.setCurrentProgram (1);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto png = outDir.getChildFile ("editor.png");
        png.deleteFile();
        FileOutputStream os (png);
        PNGImageFormat().writeImageToStream (img, os);
        check (img.getWidth() > 0, "editor renders (" + png.getFullPathName() + ")");
    }

    std::cout << (failures == 0 ? "ALL PASSED\n" : String (failures) + " FAILED\n");
    return failures == 0 ? 0 : 1;
}
