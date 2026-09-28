#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Plain parameter snapshot. The processor fills this from its APVTS each block;
// the offline renderer (waveform view, WAV export) fills it once.
struct Params808
{
    // Oscillator
    float tune       = 0.0f;    // semitones
    float fine       = 0.0f;    // cents
    float shape      = 0.15f;   // 0 = pure sine, 1 = heavily rounded square
    float sub        = 0.0f;    // sub-octave sine level

    // Punch (pitch envelope + transient)
    float pitchAmt   = 14.0f;   // semitones above the note at trigger
    float pitchDecay = 45.0f;   // ms time constant
    float click      = 0.25f;
    float clickTone  = 0.5f;

    // Amp envelope
    float attack     = 1.0f;    // ms
    float decay      = 1400.0f; // ms (to -60 dB of the gap to sustain)
    float sustain    = 0.35f;
    float release    = 250.0f;  // ms

    // Drive
    float drive      = 0.25f;   // 0..1
    int   driveMode  = 0;       // 0 Soft, 1 Hard, 2 Tube, 3 Fold, 4 Crush
    float driveMix   = 1.0f;
    float tone       = 9000.0f; // low-pass cutoff (Hz)

    // Glide / performance
    float glide      = 90.0f;   // ms
    int   glideMode  = 1;       // 0 Off, 1 Legato, 2 Always
    float bendRange  = 12.0f;   // semitones
    float output     = -3.0f;   // dB
};

//==============================================================================
// One monophonic 808 voice running at the (oversampled) engine rate.
class Voice808
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        phase = subPhase = 0.0;
        env = 0.0f;
        stage = Stage::Idle;
        pitchEnv = 0.0f;
        clickEnv = 0.0f;
        clickLp = 0.0f;
        glideRemaining = 0;
        lpIc1 = lpIc2 = 0.0f;
        dcX = dcY = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isGateOn() const noexcept { return stage == Stage::Attack || stage == Stage::Decay; }

    void setParams (const Params808& p) noexcept
    {
        params = p;
        attackInc    = 1.0f / std::max (1.0f, (float) (p.attack * 0.001 * sr));
        decayCoef    = timeToCoef (p.decay);
        releaseCoef  = timeToCoef (p.release);
        pitchCoef    = (float) std::exp (-1.0 / (std::max (1.0f, p.pitchDecay) * 0.001 * sr));
        clickCoef    = (float) std::exp (-1.0 / (0.004 * sr));
        const auto clickHz = 800.0 * std::pow (15.0, (double) p.clickTone);
        clickLpCoef  = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * clickHz / sr));
        driveGain    = juce::Decibels::decibelsToGain (p.drive * 36.0f);

        const auto g = (float) std::tan (juce::MathConstants<double>::pi
                                         * juce::jlimit (20.0, sr * 0.45, (double) p.tone) / sr);
        const float k = 1.4142f; // Butterworth
        lpA1 = 1.0f / (1.0f + g * (g + k));
        lpA2 = g * lpA1;
        lpA3 = g * lpA2;
    }

    void setPitchBend (float semitones) noexcept { bend = semitones; }

    // Start a note. legato = keep envelope running and just glide to the new pitch.
    void noteOn (int note, float velocity, bool glideToIt, bool legato)
    {
        const auto target = (float) note;
        const auto glideSamples = (int) (params.glide * 0.001 * sr);

        if (glideToIt && isActive() && glideSamples > 0)
        {
            targetNote = target;
            glideRemaining = glideSamples;
            glideStep = (targetNote - currentNote) / (float) glideSamples;
        }
        else
        {
            currentNote = targetNote = target;
            glideRemaining = 0;
        }

        if (! legato)
        {
            velGain = 0.35f + 0.65f * velocity;
            pitchEnv = params.pitchAmt;
            clickEnv = 1.0f;
            // Restart from zero phase so every hit has the same transient.
            // If the voice is still ringing, keep the phase to avoid a click.
            if (! isActive())
                phase = subPhase = 0.0;
            stage = Stage::Attack;
        }
    }

    void noteOff()
    {
        if (isActive())
            stage = Stage::Release;
    }

    float currentFrequency() const noexcept
    {
        const auto n = currentNote + params.tune + params.fine * 0.01f + pitchEnv + bend;
        return 440.0f * std::exp2 ((n - 69.0f) / 12.0f);
    }

    float processSample() noexcept
    {
        if (stage == Stage::Idle)
            return 0.0f;

        // Glide
        if (glideRemaining > 0)
        {
            currentNote += glideStep;
            if (--glideRemaining == 0)
                currentNote = targetNote;
        }

        // Oscillators
        const auto freq = std::min (currentFrequency(), (float) (sr * 0.45));
        phase += freq / sr;
        phase -= std::floor (phase);
        subPhase += 0.5 * freq / sr;
        subPhase -= std::floor (subPhase);

        const auto twoPi = juce::MathConstants<double>::twoPi;
        auto osc = (float) std::sin (twoPi * phase);
        if (params.shape > 0.001f)
        {
            // Rounds the sine toward a square, the classic "tuned-up 808" sound.
            const auto k = 1.0f + params.shape * 9.0f;
            osc = std::tanh (osc * k) / std::tanh (k);
        }
        osc += params.sub * (float) std::sin (twoPi * subPhase);

        // Click transient: filtered noise burst
        float click = 0.0f;
        if (clickEnv > 1.0e-4f)
        {
            const auto noise = random.nextFloat() * 2.0f - 1.0f;
            clickLp += clickLpCoef * (noise - clickLp);
            click = (clickLp * 2.5f + 0.6f * (float) std::sin (twoPi * phase * 4.0)) * clickEnv * params.click;
            clickEnv *= clickCoef;
        }

        pitchEnv *= pitchCoef;

        // Amp envelope
        switch (stage)
        {
            case Stage::Attack:
                env += attackInc;
                if (env >= 1.0f) { env = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                env = params.sustain + (env - params.sustain) * decayCoef;
                if (params.sustain <= 0.0f && env < 1.0e-5f) { stage = Stage::Idle; env = 0.0f; }
                break;
            case Stage::Release:
                env *= releaseCoef;
                if (env < 1.0e-5f) { stage = Stage::Idle; env = 0.0f; }
                break;
            case Stage::Idle:
                break;
        }

        const auto amp = env * velGain;
        auto x = (osc + click) * amp;

        // Drive
        if (params.drive > 0.0f || params.driveMode == 4)
        {
            const auto wet = shapeDrive (x * driveGain, amp);
            x = x + params.driveMix * (wet - x);
        }

        // DC blocker (asymmetric modes add offset)
        const auto y = x - dcX + 0.9995f * dcY;
        dcX = x; dcY = y;
        x = y;

        // Tone: 2-pole TPT low-pass
        const auto v3 = x - lpIc2;
        const auto v1 = lpA1 * lpIc1 + lpA2 * v3;
        const auto v2 = lpIc2 + lpA2 * lpIc1 + lpA3 * v3;
        lpIc1 = 2.0f * v1 - lpIc1;
        lpIc2 = 2.0f * v2 - lpIc2;
        return v2;
    }

private:
    enum class Stage { Idle, Attack, Decay, Release };

    float timeToCoef (float ms) const
    {
        // Reaches -60 dB of the remaining distance after `ms`.
        return (float) std::exp (std::log (0.001) / (std::max (1.0f, ms) * 0.001 * sr));
    }

    float shapeDrive (float x, float level) noexcept
    {
        switch (params.driveMode)
        {
            case 1: return juce::jlimit (-1.0f, 1.0f, x) * 0.9f;                       // Hard
            case 2: return x >= 0.0f ? std::tanh (x)                                     // Tube (asymmetric)
                                     : std::tanh (0.6f * x) / 0.6f * 0.55f;
            case 3: return std::sin (x * juce::MathConstants<float>::halfPi) * 0.9f;    // Fold
            case 4:                                                                     // Crush
            {
                // Quantise relative to the note's current level so the tail keeps the same
                // grit as it fades, instead of collapsing into a 1-step pulse buzz.
                const auto norm = std::max (1.0e-6f, std::tanh (level * driveGain));
                const auto levels = std::exp2 (12.0f - params.drive * 9.0f);
                return std::round (std::tanh (x) / norm * levels) / levels * norm;
            }
            default: return std::tanh (x);                                              // Soft
        }
    }

    double sr = 44100.0;
    Params808 params;
    juce::Random random { 808 };

    double phase = 0.0, subPhase = 0.0;
    float currentNote = 36.0f, targetNote = 36.0f, glideStep = 0.0f, bend = 0.0f;
    int glideRemaining = 0;

    Stage stage = Stage::Idle;
    float env = 0.0f, velGain = 1.0f;
    float attackInc = 1.0f, decayCoef = 0.999f, releaseCoef = 0.999f;
    float pitchEnv = 0.0f, pitchCoef = 0.99f;
    float clickEnv = 0.0f, clickCoef = 0.99f, clickLp = 0.0f, clickLpCoef = 0.5f;
    float driveGain = 1.0f;
    float lpA1 = 0, lpA2 = 0, lpA3 = 0, lpIc1 = 0, lpIc2 = 0;
    float dcX = 0, dcY = 0;
};

//==============================================================================
// Monophonic engine: note stack, glide rules, oversampling, output gain.
// Shared by the realtime processor and the offline renderer so both sound identical.
class Engine808
{
public:
    static constexpr int oversamplingOrder = 2; // 4x

    void prepare (double sampleRate, int maxBlockSize)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, oversamplingOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) maxBlockSize);
        voice.prepare (sampleRate * (1 << oversamplingOrder));
        voice.setParams (params);
        scratch.setSize (1, maxBlockSize);
        pending.assign ((size_t) (maxBlockSize << oversamplingOrder), 0.0f);
        heldNotes.clear();
        heldNotes.reserve (128);
        outGain.reset (sampleRate, 0.02);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.output));
    }

    void reset()
    {
        outGain.setCurrentAndTargetValue (outGain.getTargetValue());
        voice.reset();
        heldNotes.clear();
        if (oversampler) oversampler->reset();
    }

    void setParams (const Params808& p)
    {
        params = p;
        voice.setParams (p);
        voice.setPitchBend (bendNorm * p.bendRange);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (p.output));
    }

    // Renders into every channel of `buffer` (overwrites), consuming `midi`.
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int numSamples = buffer.getNumSamples();
        jassert (numSamples <= scratch.getNumSamples());
        auto* out = scratch.getWritePointer (0);
        int pos = 0;

        for (const auto metadata : midi)
        {
            const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
            renderSegment (pos, eventPos);
            pos = eventPos;
            handleMidi (metadata.getMessage());
        }
        renderSegment (pos, numSamples);

        // Down-sample the whole block in one go
        float* chans[] = { out };
        juce::dsp::AudioBlock<float> hostBlock (chans, 1, (size_t) numSamples);
        downsample (hostBlock);

        for (int i = 0; i < numSamples; ++i)
            out[i] *= outGain.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, scratch, 0, 0, numSamples);
    }

    bool isSounding() const noexcept { return voice.isActive(); }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const bool wasHeld = ! heldNotes.empty();
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
            heldNotes.push_back (m.getNoteNumber());

            const bool glide  = params.glideMode == 2 || (params.glideMode == 1 && wasHeld);
            const bool legato = params.glideMode != 0 && wasHeld && voice.isGateOn();
            voice.noteOn (m.getNoteNumber(), m.getFloatVelocity(), glide, legato);
        }
        else if (m.isNoteOff())
        {
            const auto n = m.getNoteNumber();
            const bool wasTop = ! heldNotes.empty() && heldNotes.back() == n;
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());

            if (heldNotes.empty())
                voice.noteOff();
            else if (wasTop)
                voice.noteOn (heldNotes.back(), 0.8f, params.glideMode != 0, true); // slide back
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes.clear();
            voice.noteOff();
        }
        else if (m.isPitchWheel())
        {
            bendNorm = (m.getPitchWheelValue() - 8192) / 8192.0f;
            voice.setPitchBend (bendNorm * params.bendRange);
        }
    }

private:
    // The voice runs at the oversampled rate; samples are collected in `pending`
    // and pushed through the oversampler's down-sampling filter once per block.
    void renderSegment (int start, int end)
    {
        const int factor = 1 << oversamplingOrder;
        for (int i = start * factor; i < end * factor; ++i)
            pending[(size_t) i] = voice.processSample();
    }

    void downsample (juce::dsp::AudioBlock<float>& hostBlock)
    {
        hostBlock.clear();
        auto up = oversampler->processSamplesUp (hostBlock);
        auto* u = up.getChannelPointer (0);
        const auto n = (int) up.getNumSamples();
        for (int i = 0; i < n; ++i)
            u[i] = pending[(size_t) i];
        oversampler->processSamplesDown (hostBlock);
    }

    Params808 params;
    Voice808 voice;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> scratch;
    std::vector<float> pending;
    std::vector<int> heldNotes;
    float bendNorm = 0.0f;
    juce::SmoothedValue<float> outGain;
};

//==============================================================================
// Renders one note of the current patch to a buffer (used by the waveform view and WAV export).
struct OfflineRender808
{
    static juce::AudioBuffer<float> render (const Params808& p, int note, double holdSeconds,
                                            double sampleRate, int numChannels = 1,
                                            double maxSeconds = 12.0)
    {
        constexpr int block = 512;
        Engine808 engine;
        engine.prepare (sampleRate, block);
        engine.setParams (p);
        engine.reset();

        const auto holdSamples = (int64_t) (holdSeconds * sampleRate);
        const auto maxSamples  = (int64_t) (maxSeconds * sampleRate);

        juce::AudioBuffer<float> result (numChannels, (int) maxSamples);
        result.clear();
        juce::AudioBuffer<float> tmp (numChannels, block);

        int64_t pos = 0;
        bool noteOffSent = false;
        int silentBlocks = 0;
        while (pos < maxSamples)
        {
            const int n = (int) std::min<int64_t> (block, maxSamples - pos);
            tmp.setSize (numChannels, n, false, false, true);
            juce::MidiBuffer midi;
            if (pos == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
            if (! noteOffSent && holdSamples < pos + n)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, note), (int) std::max<int64_t> (0, holdSamples - pos));
                noteOffSent = true;
            }
            engine.process (tmp, midi);
            for (int ch = 0; ch < numChannels; ++ch)
                result.copyFrom (ch, (int) pos, tmp, ch, 0, n);
            pos += n;

            if (noteOffSent && ! engine.isSounding())
                if (++silentBlocks > 4) // let the oversampling filter tail out
                    break;
        }

        // Trim trailing near-silence
        auto len = (int) pos;
        while (len > 1 && std::abs (result.getSample (0, len - 1)) < 1.0e-5f)
            --len;
        result.setSize (numChannels, std::max (1, len), true, true, true);
        return result;
    }
};
