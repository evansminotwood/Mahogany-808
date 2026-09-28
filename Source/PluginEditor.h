#pragma once

#include "PluginProcessor.h"

//==============================================================================
namespace Colours808
{
    const juce::Colour background { 0xff121316 };
    const juce::Colour panel      { 0xff1c1e23 };
    const juce::Colour panelEdge  { 0xff2a2d34 };
    const juce::Colour accent     { 0xffff7a1a };
    const juce::Colour accentDim  { 0xff7a3d12 };
    const juce::Colour text       { 0xffe8e6e3 };
    const juce::Colour textDim    { 0xff8d9098 };
}

class LookAndFeel808 final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel808();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
};

//==============================================================================
// Right-click (or ctrl-click) calls onReset instead of the normal mouse handling.
class ResettableSlider final : public juce::Slider
{
public:
    std::function<void()> onReset;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) onReset(); else Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

class ResettableComboBox final : public juce::ComboBox
{
public:
    std::function<void()> onReset;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) onReset(); else ComboBox::mouseDown (e); }
};

// A labelled rotary knob attached to one parameter. Right-click resets it to the preset's value.
class Knob final : public juce::Component
{
public:
    Knob (Forge808Processor& processor, const juce::String& paramID, const juce::String& title);
    void resized() override;

private:
    ResettableSlider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A labelled drop-down attached to a choice/int parameter. Right-click resets it to the preset's value.
class Choice final : public juce::Component
{
public:
    Choice (Forge808Processor& processor, const juce::String& paramID, const juce::String& title);
    void resized() override;

private:
    ResettableComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
// Shows the rendered one-shot of the current patch.
class WaveformView final : public juce::Component
{
public:
    void setBuffer (juce::AudioBuffer<float> newBuffer, double sampleRate, double holdSeconds, const juce::String& noteName);
    void paint (juce::Graphics&) override;

private:
    juce::AudioBuffer<float> buffer;
    double rate = 22050.0, hold = 1.0;
    juce::String note;
};

// Drag this into FL Studio's playlist/channel rack to drop the rendered WAV.
class DragChip final : public juce::Component
{
public:
    explicit DragChip (Forge808Processor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    Forge808Processor& processor;
    bool dragging = false;
};

//==============================================================================
class Forge808Editor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit Forge808Editor (Forge808Processor&);
    ~Forge808Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> items;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void refreshPreview();
    void exportClicked();
    void savePreset();
    void loadPreset();
    void stepPreset (int delta);
    void updatePresetBox();

    Forge808Processor& processor;
    LookAndFeel808 lnf;

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<Section> sections;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, loadButton { "Load" };
    juce::TextButton exportButton { "Export WAV" };
    juce::ToggleButton normalizeToggle { "Normalize" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> normalizeAttachment;
    DragChip dragChip;
    WaveformView waveform;
    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String loadedPresetName;

    Params808 lastParams;
    int lastNote = -1;
    float lastLength = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Forge808Editor)
};
