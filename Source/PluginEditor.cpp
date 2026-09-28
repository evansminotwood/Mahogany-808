#include "PluginEditor.h"

using namespace juce;

//==============================================================================
LookAndFeel808::LookAndFeel808()
{
    setColour (Slider::textBoxTextColourId, Colours808::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, Colours808::text);
    setColour (ComboBox::backgroundColourId, Colours808::panelEdge);
    setColour (ComboBox::textColourId, Colours808::text);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (ComboBox::arrowColourId, Colours808::accent);
    setColour (PopupMenu::backgroundColourId, Colours808::panel);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours808::accentDim);
    setColour (PopupMenu::textColourId, Colours808::text);
    setColour (TextButton::buttonColourId, Colours808::panelEdge);
    setColour (TextButton::textColourOffId, Colours808::text);
    setColour (ToggleButton::textColourId, Colours808::text);
    setColour (ToggleButton::tickColourId, Colours808::accent);
    setColour (ToggleButton::tickDisabledColourId, Colours808::textDim);
}

void LookAndFeel808::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, Slider&)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto trackWidth = jmax (3.0f, radius * 0.14f);
    const auto arcRadius = radius - trackWidth * 0.5f;

    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours808::panelEdge);
    g.strokePath (track, PathStrokeType (trackWidth, PathStrokeType::curved, PathStrokeType::rounded));

    Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (Colours808::accent);
    g.strokePath (value, PathStrokeType (trackWidth, PathStrokeType::curved, PathStrokeType::rounded));

    const auto knobRadius = arcRadius - trackWidth * 1.4f;
    g.setColour (Colour (0xff2b2e35));
    g.fillEllipse (Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));

    const auto tip = centre.getPointOnCircumference (knobRadius * 0.8f, angle);
    const auto base = centre.getPointOnCircumference (knobRadius * 0.25f, angle);
    g.setColour (Colours808::text);
    g.drawLine ({ base, tip }, 2.5f);
}

void LookAndFeel808::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getProperties().getWithDefault ("accent", false) ? Colours808::accent : Colours808::panelEdge;
    if (down)      c = c.darker (0.2f);
    else if (over) c = c.brighter (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
}

void LookAndFeel808::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);

    Path arrow;
    const auto ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Colours808::accent);
    g.fillPath (arrow);
}

//==============================================================================
Knob::Knob (Forge808Processor& processor, const String& paramID, const String& title)
{
    auto& state = processor.apvts;
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 76, 16);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Colours808::text);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.onReset = [&processor, paramID] { processor.resetToPresetDefault (paramID); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);

    label.setText (title.toUpperCase(), dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (FontOptions (11.0f, Font::bold));
    label.setColour (Label::textColourId, Colours808::textDim);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (Forge808Processor& processor, const String& paramID, const String& title)
{
    auto& state = processor.apvts;
    auto* param = state.getParameter (paramID);
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (param))
        box.addItemList (choice->choices, 1);
    else if (auto* ip = dynamic_cast<AudioParameterInt*> (param))
        for (int v = ip->getRange().getStart(); v <= ip->getRange().getEnd(); ++v)
            box.addItem (static_cast<AudioProcessorParameter*> (ip)->getText (ip->convertTo0to1 ((float) v), 16),
                         v - ip->getRange().getStart() + 1);

    box.setJustificationType (Justification::centred);
    box.onReset = [&processor, paramID] { processor.resetToPresetDefault (paramID); };
    addAndMakeVisible (box);
    attachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramID, box);

    label.setText (title.toUpperCase(), dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (FontOptions (11.0f, Font::bold));
    label.setColour (Label::textColourId, Colours808::textDim);
    addAndMakeVisible (label);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.withSizeKeepingCentre (jmin (r.getWidth() - 6, 80), 26));
}

//==============================================================================
void WaveformView::setBuffer (AudioBuffer<float> newBuffer, double sampleRate, double holdSeconds, const String& noteName)
{
    buffer = std::move (newBuffer);
    rate = sampleRate;
    hold = holdSeconds;
    note = noteName;
    repaint();
}

void WaveformView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0c0e));
    g.fillRoundedRectangle (r, 8.0f);

    const auto plot = r.reduced (10.0f, 8.0f);
    const auto midY = plot.getCentreY();
    g.setColour (Colours808::panelEdge);
    g.drawHorizontalLine (roundToInt (midY), plot.getX(), plot.getRight());

    const int n = buffer.getNumSamples();
    if (n <= 1)
        return;

    const auto duration = n / rate;
    const auto* d = buffer.getReadPointer (0);
    const auto peak = jmax (0.001f, buffer.getMagnitude (0, 0, n));
    const auto scale = plot.getHeight() * 0.5f / jmax (1.0f, peak);

    // Release marker
    if (hold < duration)
    {
        const auto hx = plot.getX() + (float) (hold / duration) * plot.getWidth();
        g.setColour (Colours808::textDim.withAlpha (0.35f));
        g.drawVerticalLine (roundToInt (hx), plot.getY(), plot.getBottom());
    }

    // Min/max per pixel column
    Path fill;
    const int cols = jmax (1, (int) plot.getWidth());
    std::vector<float> hi ((size_t) cols), lo ((size_t) cols);
    for (int c = 0; c < cols; ++c)
    {
        const int s0 = (int) ((int64) c * n / cols);
        const int s1 = jmax (s0 + 1, (int) ((int64) (c + 1) * n / cols));
        auto mm = FloatVectorOperations::findMinAndMax (d + s0, s1 - s0);
        hi[(size_t) c] = midY - mm.getEnd() * scale;
        lo[(size_t) c] = midY - mm.getStart() * scale;
    }
    fill.startNewSubPath (plot.getX(), hi[0]);
    for (int c = 1; c < cols; ++c) fill.lineTo (plot.getX() + (float) c, hi[(size_t) c]);
    for (int c = cols - 1; c >= 0; --c) fill.lineTo (plot.getX() + (float) c, lo[(size_t) c]);
    fill.closeSubPath();

    g.setGradientFill (ColourGradient (Colours808::accent, plot.getX(), 0.0f,
                                       Colours808::accent.withAlpha (0.55f), plot.getRight(), 0.0f, false));
    g.fillPath (fill);

    g.setColour (Colours808::textDim);
    g.setFont (FontOptions (12.0f));
    g.drawText (note + "  " + String (duration, 2) + " s" + (peak > 1.0f ? "   (clipping, lower Output)" : ""),
                plot.toNearestInt().removeFromTop (16), Justification::topRight);
}

//==============================================================================
void DragChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (dragging ? Colours808::accent : Colours808::panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
    g.setColour (Colours808::text);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText ("DRAG WAV", getLocalBounds(), Justification::centred);
}

void DragChip::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;

    dragging = true;
    repaint();

    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("808Forge");
    dir.createDirectory();
    auto file = dir.getChildFile (processor.suggestedFileName());

    if (processor.exportWav (file).isEmpty())
        DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                             [safe = SafePointer<DragChip> (this)]
                                                             {
                                                                 if (safe != nullptr) { safe->dragging = false; safe->repaint(); }
                                                             });
}

//==============================================================================
Forge808Editor::Forge808Editor (Forge808Processor& p)
    : AudioProcessorEditor (p), processor (p), dragChip (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    auto knob = [&] (const char* id, const char* title) -> Component*
    {
        controls.push_back (std::make_unique<Knob> (p, id, title));
        return controls.back().get();
    };
    auto choice = [&] (const char* id, const char* title) -> Component*
    {
        controls.push_back (std::make_unique<Choice> (p, id, title));
        return controls.back().get();
    };

    using namespace ParamIDs;
    sections = {
        { "OSCILLATOR", { knob (tune, "Tune"), knob (fine, "Fine"), knob (shape, "Shape"), knob (sub, "Sub") }, {} },
        { "PUNCH",      { knob (pitchAmt, "Punch"), knob (pitchDecay, "Punch Time"), knob (click, "Click"), knob (clickTone, "Click Tone") }, {} },
        { "GLIDE",      { knob (glide, "Glide"), choice (glideMode, "Mode"), knob (bendRange, "Bend") }, {} },
        { "ENVELOPE",   { knob (attack, "Attack"), knob (decay, "Decay"), knob (sustain, "Sustain"), knob (release, "Release") }, {} },
        { "DRIVE",      { knob (drive, "Drive"), choice (driveMode, "Type"), knob (driveMix, "Mix"), knob (tone, "Tone") }, {} },
        { "OUTPUT / RENDER", { knob (output, "Output"), choice (renderNote, "Note"), knob (renderLength, "Length") }, {} },
    };
    for (auto& c : controls)
        addAndMakeVisible (*c);

    // Header
    for (int i = 0; i < p.getNumPrograms(); ++i)
        presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const auto idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && (idx != processor.getCurrentProgram() || loadedPresetName.isNotEmpty()))
        {
            loadedPresetName.clear();
            processor.setCurrentProgram (idx);
        }
    };
    updatePresetBox();
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    exportButton.getProperties().set ("accent", true);
    exportButton.onClick = [this] { exportClicked(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton, &exportButton })
        addAndMakeVisible (b);

    normalizeAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, normalize, normalizeToggle);
    addAndMakeVisible (normalizeToggle);
    addAndMakeVisible (dragChip);
    addAndMakeVisible (waveform);

    keyboard.setAvailableRange (24, 84);
    keyboard.setLowestVisibleKey (24);
    keyboard.setOctaveForMiddleC (5); // FL Studio naming: middle C = C5
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Colours808::accent);
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours808::accent.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    setSize (980, 624);
    refreshPreview();
    startTimerHz (15);
}

Forge808Editor::~Forge808Editor()
{
    setLookAndFeel (nullptr);
}

void Forge808Editor::paint (Graphics& g)
{
    g.fillAll (Colours808::background);

    const Font logoFont (FontOptions (24.0f, Font::bold));
    const auto nameWidth = roundToInt (GlyphArrangement::getStringWidth (logoFont, "MAHOGANY "));
    g.setFont (logoFont);
    g.setColour (Colours808::text);
    g.drawText ("MAHOGANY", 18, 12, nameWidth, 32, Justification::centredLeft);
    g.setColour (Colours808::accent);
    g.drawText ("808", 18 + nameWidth, 12, 60, 32, Justification::centredLeft);

    for (const auto& sec : sections)
    {
        g.setColour (Colours808::panel);
        g.fillRoundedRectangle (sec.bounds.toFloat(), 8.0f);
        g.setColour (Colours808::accent.withAlpha (0.85f));
        g.setFont (FontOptions (11.0f, Font::bold));
        g.drawText (sec.title, sec.bounds.reduced (12, 6).removeFromTop (14), Justification::topLeft);
    }
}

void Forge808Editor::resized()
{
    auto r = getLocalBounds().reduced (16, 0);

    // Header
    auto header = r.removeFromTop (56).withTrimmedTop (12).withTrimmedBottom (12);
    header.removeFromLeft (200);
    presetBox.setBounds (header.removeFromLeft (190));
    header.removeFromLeft (6);
    prevButton.setBounds (header.removeFromLeft (28));
    header.removeFromLeft (4);
    nextButton.setBounds (header.removeFromLeft (28));
    header.removeFromLeft (10);
    saveButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    loadButton.setBounds (header.removeFromLeft (56));

    dragChip.setBounds (header.removeFromRight (96));
    header.removeFromRight (8);
    exportButton.setBounds (header.removeFromRight (104));
    header.removeFromRight (8);
    normalizeToggle.setBounds (header.removeFromRight (100));

    waveform.setBounds (r.removeFromTop (150));
    r.removeFromTop (10);

    keyboard.setBounds (r.removeFromBottom (78).withTrimmedBottom (14));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f); // 36 white keys in 24..84
    r.removeFromBottom (8);

    const int gap = 10;
    const int rowH = (r.getHeight() - gap) / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = r.removeFromTop (rowH);
        r.removeFromTop (gap);

        int slots = 0;
        for (int i = 0; i < 3; ++i)
            slots += (int) sections[(size_t) (row * 3 + i)].items.size();
        const float slotW = (float) (rowArea.getWidth() - gap * 2) / (float) slots;

        for (int i = 0; i < 3; ++i)
        {
            auto& sec = sections[(size_t) (row * 3 + i)];
            const int w = i == 2 ? rowArea.getWidth() : roundToInt (slotW * (float) sec.items.size());
            sec.bounds = rowArea.removeFromLeft (w);
            rowArea.removeFromLeft (gap);

            auto inner = sec.bounds.reduced (6, 6).withTrimmedTop (16);
            const int itemW = inner.getWidth() / (int) sec.items.size();
            for (auto* c : sec.items)
                c->setBounds (inner.removeFromLeft (itemW));
        }
    }
}

void Forge808Editor::timerCallback()
{
    const auto params = processor.snapshotParams();
    const auto note = (int) processor.apvts.getRawParameterValue (ParamIDs::renderNote)->load();
    const auto length = processor.apvts.getRawParameterValue (ParamIDs::renderLength)->load();

    if (std::memcmp (&params, &lastParams, sizeof (Params808)) != 0 || note != lastNote || ! approximatelyEqual (length, lastLength))
        refreshPreview();

    if (presetBox.getSelectedItemIndex() != processor.getCurrentProgram() && loadedPresetName.isEmpty())
        updatePresetBox();
}

void Forge808Editor::refreshPreview()
{
    lastParams = processor.snapshotParams();
    lastNote = (int) processor.apvts.getRawParameterValue (ParamIDs::renderNote)->load();
    lastLength = processor.apvts.getRawParameterValue (ParamIDs::renderLength)->load();

    constexpr double previewRate = 22050.0;
    waveform.setBuffer (processor.renderPreview (previewRate), previewRate, lastLength,
                        MidiMessage::getMidiNoteName (lastNote, true, true, 5));
}

void Forge808Editor::exportClicked()
{
    auto startDir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (processor.suggestedFileName());
    chooser = std::make_unique<FileChooser> ("Export 808 as WAV", startDir, "*.wav");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                              | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == File())
                                  return;
                              file = file.withFileExtension ("wav");
                              const auto error = processor.exportWav (file);
                              if (error.isNotEmpty())
                                  AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Export failed", error);
                          });
}

void Forge808Editor::savePreset()
{
    auto startDir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("My 808.808preset");
    chooser = std::make_unique<FileChooser> ("Save preset", startDir, "*.808preset");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                              | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == File())
                                  return;
                              file = file.withFileExtension ("808preset");
                              if (auto xml = processor.apvts.copyState().createXml())
                                  if (xml->writeTo (file))
                                  {
                                      processor.captureUserPresetDefaults();
                                      loadedPresetName = file.getFileNameWithoutExtension();
                                      presetBox.setText (loadedPresetName, dontSendNotification);
                                  }
                          });
}

void Forge808Editor::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Load preset", File::getSpecialLocation (File::userDocumentsDirectory), "*.808preset");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              if (auto xml = parseXML (file))
                                  if (xml->hasTagName (processor.apvts.state.getType()))
                                  {
                                      processor.apvts.replaceState (ValueTree::fromXml (*xml));
                                      processor.captureUserPresetDefaults();
                                      loadedPresetName = file.getFileNameWithoutExtension();
                                      presetBox.setText (loadedPresetName, dontSendNotification);
                                  }
                          });
}

void Forge808Editor::stepPreset (int delta)
{
    const int n = processor.getNumPrograms();
    loadedPresetName.clear();
    processor.setCurrentProgram ((processor.getCurrentProgram() + delta + n) % n);
    updatePresetBox();
}

void Forge808Editor::updatePresetBox()
{
    presetBox.setSelectedItemIndex (processor.getCurrentProgram(), dontSendNotification);
}
