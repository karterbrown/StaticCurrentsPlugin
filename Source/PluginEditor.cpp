/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StaticCurrentsPluginAudioProcessorEditor::StaticCurrentsPluginAudioProcessorEditor (StaticCurrentsPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Load image from Resources folder - try multiple potential locations
    juce::File imageFile;
    
    // Try project root Resources folder
    imageFile = juce::File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("SCimage.png");
    
    if (!imageFile.existsAsFile())
        imageFile = juce::File::getCurrentWorkingDirectory().getChildFile("Resources").getChildFile("SCimage.png");
    
    if (!imageFile.existsAsFile())
        imageFile = juce::File("/Users/karterbrown/Desktop/Dev/Plugins/StaticCurrentsPlugin/Resources/SCimage.png");
    
    if (imageFile.existsAsFile())
        logoImage = juce::ImageCache::getFromFile(imageFile);
    // Setup load button
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Select a sample file...",
                                                             juce::File{},
                                                             "*.wav;*.aif;*.aiff");
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.loadSampleFromFile (file);
                fileLabel.setText (file.getFileName(), juce::dontSendNotification);
            }
        });
    };

    addAndMakeVisible (fileLabel);
    fileLabel.setText ("No sample loaded", juce::dontSendNotification);
    fileLabel.setJustificationType (juce::Justification::centred);

    // Setup record button
    addAndMakeVisible (recordButton);
    recordButton.onClick = [this]
    {
        if (audioProcessor.isRecording())
        {
            audioProcessor.stopRecording();
            fileLabel.setText ("Recorded sample loaded", juce::dontSendNotification);
        }
        else
        {
            audioProcessor.startRecording();
            fileLabel.setText ("Recording...", juce::dontSendNotification);
        }
        updateRecordButton();
    };

    // Setup bypass button
    addAndMakeVisible (bypassButton);
    bypassButton.onClick = [this]
    {
        bool bypassed = !(*audioProcessor.getBypassParameter());
        *audioProcessor.getBypassParameter() = bypassed;
        bypassButton.setButtonText (bypassed ? "Bypass (ON)" : "Bypass");
        bypassButton.setColour (juce::TextButton::buttonColourId,
                                bypassed ? juce::Colours::orange
                                         : getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    };

    // Setup play button
    addAndMakeVisible (playButton);
    playButton.onClick = [this]
    {
        if (isPlaying)
        {
            audioProcessor.stopSamplePlayback();
            playButton.setButtonText ("Play Sample");
            playButton.setColour (juce::TextButton::buttonColourId,
                                  getLookAndFeel().findColour (juce::TextButton::buttonColourId));
            isPlaying = false;
        }
        else
        {
            audioProcessor.triggerSamplePlayback();
            playButton.setButtonText ("Pause");
            playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::green);
            isPlaying = true;
        }
    };

    // Setup generate button
    addAndMakeVisible (generateButton);
    generateButton.onClick = [this]
    {
        juce::PopupMenu formatMenu;
        formatMenu.addItem (1, "WAV (24-bit)");
        formatMenu.addItem (2, "WAV (16-bit)");
        formatMenu.addItem (3, "MP3");
        formatMenu.addItem (4, "OGG Vorbis");
        formatMenu.addItem (5, "FLAC");

        formatMenu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
        {
            if (result == 0)
                return;

            juce::String extension;
            switch (result)
            {
                case 1: extension = ".wav"; break;
                case 2: extension = ".wav"; break;
                case 3: extension = ".mp3"; break;
                case 4: extension = ".ogg"; break;
                case 5: extension = ".flac"; break;
            }

            auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
            fileChooser = std::make_unique<juce::FileChooser> ("Save Processed Sample As...",
                                                                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                                "*" + extension);

            fileChooser->launchAsync (chooserFlags, [this, extension] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File{})
                {
                    if (!file.hasFileExtension (extension))
                        file = file.withFileExtension (extension);

                    audioProcessor.exportProcessedSample (file);
                }
            });
        });
    };

    // Setup progress slider
    addAndMakeVisible (progressSlider);
    addAndMakeVisible (progressLabel);
    progressSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    progressSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    progressSlider.setRange (0.0, 1.0, 0.001);
    progressSlider.setValue (0.0);
    progressSlider.onDragStart = [this]
    {
        if (isPlaying)
            audioProcessor.stopSamplePlayback();
    };
    progressSlider.onValueChange = [this]
    {
        if (progressSlider.isMouseButtonDown())
        {
            float seekPos = static_cast<float> (progressSlider.getValue());
            audioProcessor.seekToPosition (seekPos);
        }
    };
    progressSlider.onDragEnd = [this]
    {
        if (isPlaying)
            audioProcessor.triggerSamplePlayback();
    };

    // Basic parameters
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (gainLabel);
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    gainSlider.setRange (0.0, 1.0, 0.01);
    gainSlider.setValue (0.7);
    gainSlider.onValueChange = [this] { *audioProcessor.getGainParameter() = static_cast<float> (gainSlider.getValue()); };

    addAndMakeVisible (pitchSlider);
    addAndMakeVisible (pitchLabel);
    pitchSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    pitchSlider.setRange (0.5, 2.0, 0.01);
    pitchSlider.setValue (1.0);
    pitchSlider.setTextValueSuffix (" x");
    pitchSlider.onValueChange = [this] { *audioProcessor.getPitchParameter() = static_cast<float> (pitchSlider.getValue()); };

    // Profile selector (items are initialized on first resized)
    addAndMakeVisible (profileBox);
    addAndMakeVisible (profileLabel);

    // EQ visualization
    addAndMakeVisible (eqLabel);
    addAndMakeVisible (eqVisualization);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (eqParamsLabel);
    eqParamsLabel.setText ("Gain / Pitch + Comp", juce::dontSendNotification);

    eqVisualization.onBandDragged = [this] (int bandIndex, float freq, float gain)
    {
        switch (bandIndex)
        {
            case 0:
                *audioProcessor.getHPFFreqParameter() = freq;
                break;
            case 1:
                *audioProcessor.getPeak1FreqParameter() = freq;
                *audioProcessor.getPeak1GainParameter() = gain;
                break;
            case 2:
                *audioProcessor.getPeak2FreqParameter() = freq;
                *audioProcessor.getPeak2GainParameter() = gain;
                break;
            case 3:
                *audioProcessor.getPeak3FreqParameter() = freq;
                *audioProcessor.getPeak3GainParameter() = gain;
                break;
            case 4:
                *audioProcessor.getPeak4FreqParameter() = freq;
                *audioProcessor.getPeak4GainParameter() = gain;
                break;
            case 5:
                *audioProcessor.getLPFFreqParameter() = freq;
                break;
        }
        updateEQVisualization();
    };

    eqVisualization.onQChanged = [this] (int bandIndex, float q)
    {
        switch (bandIndex)
        {
            case 1: *audioProcessor.getPeak1QParameter() = q; break;
            case 2: *audioProcessor.getPeak2QParameter() = q; break;
            case 3: *audioProcessor.getPeak3QParameter() = q; break;
            case 4: *audioProcessor.getPeak4QParameter() = q; break;
        }
        updateEQVisualization();
    };

    eqVisualization.onSlopeChanged = [this] (int bandIndex, float slope)
    {
        switch (bandIndex)
        {
            case 0: *audioProcessor.getHPFSlopeParameter() = slope; break;
            case 5: *audioProcessor.getLPFSlopeParameter() = slope; break;
        }
        updateEQVisualization();
    };

    resetButton.onClick = [this]
    {
        audioProcessor.applyProfilePreset (0);
        profileBox.setSelectedId (1, juce::dontSendNotification);
        syncSlidersFromParameters();
        updateEQVisualization();
    };

    // Saturation section
    addAndMakeVisible (saturationSectionLabel);

    auto setupSatSlider = [this] (juce::Slider& slider, juce::Label& label,
                                  float min, float max, float step, float value,
                                  std::atomic<float>* param,
                                  int saturationTypeId)
    {
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 18);
        slider.setRange (min, max, step);
        slider.setValue (value);
        slider.onValueChange = [this, param, &slider, saturationTypeId]
        {
            *param = static_cast<float> (slider.getValue());
            if (saturationTypeId > 0)
            {
                *audioProcessor.getSaturationTypeParameter() = static_cast<float> (saturationTypeId);
                *audioProcessor.getSaturationParameter() = 1.0f;
            }
        };
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (14.0f));
    };

    addAndMakeVisible (tubeLabel);
    addAndMakeVisible (transistorLabel);
    addAndMakeVisible (tapeLabel);
    addAndMakeVisible (diodeLabel);
    addAndMakeVisible (fuzzLabel);
    addAndMakeVisible (bitLabel);

    setupSatSlider (tubeDriveSlider, tubeDriveLabel, 0.0f, 10.0f, 0.1f, 0.0f, audioProcessor.getTubeDriveParameter(), 1);
    setupSatSlider (tubeWarmthSlider, tubeWarmthLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getTubeWarmthParameter(), 1);
    setupSatSlider (tubeBiasSlider, tubeBiasLabel, -1.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getTubeBiasParameter(), 1);
    setupSatSlider (tubeOutputSlider, tubeOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getTubeOutputParameter(), 1);

    setupSatSlider (transistorDriveSlider, transistorDriveLabel, 0.0f, 10.0f, 0.1f, 0.0f, audioProcessor.getTransistorDriveParameter(), 2);
    setupSatSlider (transistorBiteSlider, transistorBiteLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getTransistorBiteParameter(), 2);
    setupSatSlider (transistorClipSlider, transistorClipLabel, 0.0f, 1.0f, 0.01f, 0.5f, audioProcessor.getTransistorClipParameter(), 2);
    setupSatSlider (transistorOutputSlider, transistorOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getTransistorOutputParameter(), 2);

    setupSatSlider (tapeDriveSlider, tapeDriveLabel, 0.0f, 10.0f, 0.1f, 0.0f, audioProcessor.getTapeDriveParameter(), 3);
    setupSatSlider (tapeWowSlider, tapeWowLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getTapeWowParameter(), 3);
    setupSatSlider (tapeHissSlider, tapeHissLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getTapeHissParameter(), 3);
    setupSatSlider (tapeOutputSlider, tapeOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getTapeOutputParameter(), 3);

    setupSatSlider (diodeDriveSlider, diodeDriveLabel, 0.0f, 10.0f, 0.1f, 0.0f, audioProcessor.getDiodeDriveParameter(), 4);
    setupSatSlider (diodeAsymSlider, diodeAsymLabel, 0.0f, 1.0f, 0.01f, 0.5f, audioProcessor.getDiodeAsymParameter(), 4);
    setupSatSlider (diodeClipSlider, diodeClipLabel, 0.0f, 1.0f, 0.01f, 0.5f, audioProcessor.getDiodeClipParameter(), 4);
    setupSatSlider (diodeOutputSlider, diodeOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getDiodeOutputParameter(), 4);

    setupSatSlider (fuzzDriveSlider, fuzzDriveLabel, 0.0f, 10.0f, 0.1f, 0.0f, audioProcessor.getFuzzDriveParameter(), 5);
    setupSatSlider (fuzzGateSlider, fuzzGateLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getFuzzGateParameter(), 5);
    setupSatSlider (fuzzToneSlider, fuzzToneLabel, 0.0f, 1.0f, 0.01f, 0.5f, audioProcessor.getFuzzToneParameter(), 5);
    setupSatSlider (fuzzOutputSlider, fuzzOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getFuzzOutputParameter(), 5);

    setupSatSlider (bitDepthSlider, bitDepthLabel, 2.0f, 16.0f, 1.0f, 16.0f, audioProcessor.getBitDepthParameter(), 6);
    setupSatSlider (bitRateSlider, bitRateLabel, 1.0f, 16.0f, 1.0f, 1.0f, audioProcessor.getBitRateParameter(), 6);
    setupSatSlider (bitMixSlider, bitMixLabel, 0.0f, 1.0f, 0.01f, 0.0f, audioProcessor.getBitMixParameter(), 6);
    setupSatSlider (bitOutputSlider, bitOutputLabel, 0.0f, 2.0f, 0.01f, 1.0f, audioProcessor.getBitOutputParameter(), 6);

    // Compressor
    addAndMakeVisible (compLabel);
    addAndMakeVisible (compThreshLabel);
    addAndMakeVisible (compRatioLabel);
    addAndMakeVisible (compAttackLabel);
    addAndMakeVisible (compReleaseLabel);
    addAndMakeVisible (compMakeupLabel);
    addAndMakeVisible (compThreshSlider);
    addAndMakeVisible (compRatioSlider);
    addAndMakeVisible (compAttackSlider);
    addAndMakeVisible (compReleaseSlider);
    addAndMakeVisible (compMakeupSlider);

    compThreshSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compThreshSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    compThreshSlider.setRange (-60.0, 0.0, 0.1);
    compThreshSlider.setValue (-20.0);
    compThreshSlider.setTextValueSuffix (" dB");
    compThreshSlider.onValueChange = [this] { *audioProcessor.getCompThreshParameter() = static_cast<float> (compThreshSlider.getValue()); };

    compRatioSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compRatioSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    compRatioSlider.setRange (1.0, 20.0, 0.1);
    compRatioSlider.setValue (4.0);
    compRatioSlider.setTextValueSuffix (":1");
    compRatioSlider.onValueChange = [this] { *audioProcessor.getCompRatioParameter() = static_cast<float> (compRatioSlider.getValue()); };

    compAttackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compAttackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    compAttackSlider.setRange (0.001, 0.1, 0.001);
    compAttackSlider.setValue (0.01);
    compAttackSlider.setSkewFactorFromMidPoint (0.01);
    compAttackSlider.setTextValueSuffix (" s");
    compAttackSlider.onValueChange = [this] { *audioProcessor.getCompAttackParameter() = static_cast<float> (compAttackSlider.getValue()); };

    compReleaseSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compReleaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    compReleaseSlider.setRange (0.01, 1.0, 0.01);
    compReleaseSlider.setValue (0.1);
    compReleaseSlider.setSkewFactorFromMidPoint (0.1);
    compReleaseSlider.setTextValueSuffix (" s");
    compReleaseSlider.onValueChange = [this] { *audioProcessor.getCompReleaseParameter() = static_cast<float> (compReleaseSlider.getValue()); };

    compMakeupSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compMakeupSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    compMakeupSlider.setRange (0.0, 24.0, 0.1);
    compMakeupSlider.setValue (0.0);
    compMakeupSlider.setTextValueSuffix (" dB");
    compMakeupSlider.onValueChange = [this] { *audioProcessor.getCompMakeupParameter() = static_cast<float> (compMakeupSlider.getValue()); };

    // Setup click-to-reset listeners for all sliders
    auto addClickReset = [this] (juce::Slider& slider, double defaultValue)
    {
        auto listener = std::make_unique<ClickToResetListener>(slider, defaultValue);
        slider.addMouseListener(listener.get(), false);
        clickResetListeners.push_back(std::move(listener));
    };

    // Basic parameters
    addClickReset(gainSlider, 0.7);
    addClickReset(pitchSlider, 1.0);

    // Compressor
    addClickReset(compThreshSlider, 0.0);
    addClickReset(compRatioSlider, 1.0);
    addClickReset(compAttackSlider, 0.01);
    addClickReset(compReleaseSlider, 0.1);
    addClickReset(compMakeupSlider, 0.0);

    // Tube saturation
    addClickReset(tubeDriveSlider, 0.0);
    addClickReset(tubeWarmthSlider, 0.0);
    addClickReset(tubeBiasSlider, 0.0);
    addClickReset(tubeOutputSlider, 1.0);

    // Transistor saturation
    addClickReset(transistorDriveSlider, 0.0);
    addClickReset(transistorBiteSlider, 0.0);
    addClickReset(transistorClipSlider, 0.5);
    addClickReset(transistorOutputSlider, 1.0);

    // Tape saturation
    addClickReset(tapeDriveSlider, 0.0);
    addClickReset(tapeWowSlider, 0.0);
    addClickReset(tapeHissSlider, 0.0);
    addClickReset(tapeOutputSlider, 1.0);

    // Diode saturation
    addClickReset(diodeDriveSlider, 0.0);
    addClickReset(diodeAsymSlider, 0.5);
    addClickReset(diodeClipSlider, 0.5);
    addClickReset(diodeOutputSlider, 1.0);

    // Fuzz saturation
    addClickReset(fuzzDriveSlider, 0.0);
    addClickReset(fuzzGateSlider, 0.0);
    addClickReset(fuzzToneSlider, 0.5);
    addClickReset(fuzzOutputSlider, 1.0);

    // Bitcrush saturation
    addClickReset(bitDepthSlider, 16.0);
    addClickReset(bitRateSlider, 1.0);
    addClickReset(bitMixSlider, 0.0);
    addClickReset(bitOutputSlider, 1.0);

    startTimer (50);

    setSize (1200, 900);
    setResizable (true, true);
    // Minimum size calculated based on:
    // Height: 12px border + 180px top area + 412px content (140+8+140+8+116 for image min) + 12px border = 616px
    // Width: Minimum for 3 columns with readable controls
    setResizeLimits (900, 616, 2400, 1800);
}

StaticCurrentsPluginAudioProcessorEditor::~StaticCurrentsPluginAudioProcessorEditor()
{
    stopTimer();
    
    // Clear mouse listeners before destroying sliders
    clickResetListeners.clear();
}

//==============================================================================
void StaticCurrentsPluginAudioProcessorEditor::timerCallback()
{
    updateRecordButton();

    float length = audioProcessor.getSampleLength();
    if (length > 0.0f)
    {
        progressSlider.setEnabled (true);
        progressSlider.setRange (0.0, length, 0.01);

        if (!progressSlider.isMouseButtonDown())
        {
            float position = audioProcessor.getPlaybackPosition();
            progressSlider.setValue (position, juce::dontSendNotification);
            
            // Check if playback has finished
            if (isPlaying && position >= length - 0.1f)
            {
                isPlaying = false;
                playButton.setButtonText ("Play Sample");
                playButton.setColour (juce::TextButton::buttonColourId,
                                     getLookAndFeel().findColour (juce::TextButton::buttonColourId));
            }
        }

        float currentPos = static_cast<float> (progressSlider.getValue());
        int currentMin = static_cast<int> (currentPos) / 60;
        int currentSec = static_cast<int> (currentPos) % 60;
        int totalMin = static_cast<int> (length) / 60;
        int totalSec = static_cast<int> (length) % 60;

        progressSlider.setTextValueSuffix (juce::String::formatted (" (%d:%02d / %d:%02d)",
                                                                    currentMin, currentSec,
                                                                    totalMin, totalSec));
    }
    else
    {
        progressSlider.setEnabled (false);
        progressSlider.setValue (0.0, juce::dontSendNotification);
        progressSlider.setTextValueSuffix ("");
    }
}

//==============================================================================
void StaticCurrentsPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto drawSection = [&g] (juce::Rectangle<int> area, float fillAlpha)
    {
        if (area.isEmpty())
            return;

        auto r = area.reduced (4).toFloat();
        g.setColour (juce::Colour (0xffd4c9b8).withAlpha (fillAlpha));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xff8b6d5c).withAlpha (0.9f));
        g.drawRoundedRectangle (r, 8.0f, 1.5f);
    };

    drawSection (topSectionBounds, 0.28f);
    drawSection (eqSectionBounds, 0.30f);
    drawSection (gainSectionBounds, 0.26f);
    drawSection (compSectionBounds, 0.26f);
    drawSection (saturationSectionBounds, 0.26f);

    for (auto& area : saturationTypeBounds)
        drawSection (area, 0.18f);

    // Draw logo image below compressor
    if (logoImage.isValid() && !logoImageBounds.isEmpty())
    {
        auto imageBounds = logoImageBounds.reduced(8);
        
        // Ensure minimum size to prevent overlap
        if (imageBounds.getWidth() >= 100 && imageBounds.getHeight() >= 100)
        {
            g.drawImageWithin (logoImage,
                              imageBounds.getX(),
                              imageBounds.getY(),
                              imageBounds.getWidth(),
                              imageBounds.getHeight(),
                              juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Voice Sampler & Processor", getLocalBounds(), juce::Justification::centredTop, 1);
}

void StaticCurrentsPluginAudioProcessorEditor::resized()
{
    // Initialize profile box on first resized call (after layout established)
    if (!profileInitialized)
    {
        profileInitialized = true;

        profileBox.addItem ("-init-", 1);
        profileBox.addItem ("Wax Cylinder", 2);
        profileBox.addItem ("Vinyl", 3);
        profileBox.addItem ("Cassette", 4);
        profileBox.addItem ("Reel to Reel", 5);
        profileBox.addItem ("Neve", 6);
        profileBox.addItem ("API", 7);
        profileBox.addItem ("Blown Speaker", 8);
        profileBox.addItem ("HiFi", 9);
        profileBox.addItem ("LoFi", 10);

        profileBox.onChange = [this]
        {
            int profileID = profileBox.getSelectedId() - 1;
            audioProcessor.applyProfilePreset (profileID);
            syncSlidersFromParameters();
            updateEQVisualization();
        };

        profileBox.setSelectedId (1, juce::dontSendNotification);
        audioProcessor.applyProfilePreset (0);
        syncSlidersFromParameters();
        updateEQVisualization();
    }

    auto bounds = getLocalBounds().reduced (12);

    const int headerHeight = 32;
    const int buttonRowHeight = 44;
    const int fileLabelHeight = 24;
    const int progressRowHeight = 40;
    const int profileRowHeight = 30;
    const int gapAfterProfile = 10;

    auto topArea = bounds.removeFromTop (headerHeight + buttonRowHeight + fileLabelHeight
                                         + progressRowHeight + profileRowHeight + gapAfterProfile);
    topSectionBounds = topArea;

    auto header = topArea.removeFromTop (headerHeight);
    header.removeFromRight (12);

    auto buttonRow = topArea.removeFromTop (buttonRowHeight);
    const int buttonGap = 8;
    const int buttonWidth = juce::jmin (160, (buttonRow.getWidth() - buttonGap * 4) / 5);
    const int totalButtonsWidth = buttonWidth * 5 + buttonGap * 4;
    auto buttonStrip = buttonRow.withWidth (totalButtonsWidth)
                                 .withX (buttonRow.getX() + (buttonRow.getWidth() - totalButtonsWidth) / 2);

    loadButton.setBounds (buttonStrip.removeFromLeft (buttonWidth));
    buttonStrip.removeFromLeft (buttonGap);
    recordButton.setBounds (buttonStrip.removeFromLeft (buttonWidth));
    buttonStrip.removeFromLeft (buttonGap);
    bypassButton.setBounds (buttonStrip.removeFromLeft (buttonWidth));
    buttonStrip.removeFromLeft (buttonGap);
    playButton.setBounds (buttonStrip.removeFromLeft (buttonWidth));
    buttonStrip.removeFromLeft (buttonGap);
    generateButton.setBounds (buttonStrip.removeFromLeft (buttonWidth));

    fileLabel.setBounds (topArea.removeFromTop (fileLabelHeight).reduced (10, 0));

    auto progressRow = topArea.removeFromTop (progressRowHeight).reduced (60, 0);
    progressLabel.setBounds (progressRow.removeFromLeft (90).reduced (4));
    progressSlider.setBounds (progressRow.reduced (4));

    auto profileRow = topArea.removeFromTop (profileRowHeight).reduced (60, 0);
    const int profileLabelWidth = 90;
    const int profileBoxWidth = 220;
    const int profileTotalWidth = profileLabelWidth + 8 + profileBoxWidth;
    auto profileStrip = profileRow.withWidth (profileTotalWidth)
                                  .withX (profileRow.getX() + (profileRow.getWidth() - profileTotalWidth) / 2);
    profileLabel.setBounds (profileStrip.removeFromLeft (profileLabelWidth).reduced (4));
    profileStrip.removeFromLeft (8);
    profileBox.setBounds (profileStrip.removeFromLeft (profileBoxWidth).reduced (4));

    auto content = bounds;
    const int columnGap = 12;
    const int colWidth = (content.getWidth() - columnGap * 2) / 3;

    auto leftCol = content.removeFromLeft (colWidth);
    content.removeFromLeft (columnGap);
    auto midCol = content.removeFromLeft (colWidth);
    content.removeFromLeft (columnGap);
    auto rightCol = content;

    const int colInnerGap = 8;
    // Match section heights - distribute evenly with more space for image
    int totalHeight = leftCol.getHeight();
    const int gainHeight = juce::jmax(140, totalHeight / 5);
    const int compHeight = juce::jmax(140, totalHeight / 5);
    
    gainSectionBounds = leftCol.removeFromTop (gainHeight);
    leftCol.removeFromTop (colInnerGap);
    compSectionBounds = leftCol.removeFromTop (compHeight);
    leftCol.removeFromTop (colInnerGap);
    logoImageBounds = leftCol;  // Use all remaining space

    eqSectionBounds = midCol;
    saturationSectionBounds = rightCol;

    const int knobWidth = 90;
    const int knobGap = 10;

    auto gainArea = gainSectionBounds.reduced (8);
    const int gainTotalWidth = knobWidth * 2 + knobGap;
    auto gainStrip = gainArea.withWidth (gainTotalWidth)
                             .withX (gainArea.getX() + (gainArea.getWidth() - gainTotalWidth) / 2);
    auto knobArea = gainStrip.removeFromLeft (knobWidth);
    gainLabel.setBounds (knobArea.removeFromTop (22));
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setFont (juce::FontOptions (14.0f));
    gainSlider.setBounds (knobArea.reduced (2));
    gainStrip.removeFromLeft (knobGap);
    knobArea = gainStrip.removeFromLeft (knobWidth);
    pitchLabel.setBounds (knobArea.removeFromTop (22));
    pitchLabel.setJustificationType (juce::Justification::centred);
    pitchLabel.setFont (juce::FontOptions (14.0f));
    pitchSlider.setBounds (knobArea.reduced (2));

    auto compArea = compSectionBounds.reduced (8);
    auto compHeaderArea = compArea.removeFromTop (26);
    compLabel.setBounds (compHeaderArea);
    compLabel.setJustificationType (juce::Justification::centred);
    compLabel.setFont (juce::FontOptions (16.0f));
    
    // Calculate knob size based on available width
    int availableWidth = compArea.getWidth();
    int numKnobs = 5;
    int totalGaps = (numKnobs - 1) * knobGap;
    int compKnobWidth = juce::jmin(knobWidth, (availableWidth - totalGaps) / numKnobs);
    
    const int compTotalWidth = compKnobWidth * numKnobs + knobGap * (numKnobs - 1);
    auto compStrip = compArea.withWidth (compTotalWidth)
                             .withX (compArea.getX() + (compArea.getWidth() - compTotalWidth) / 2);

    auto compSlot = compStrip.removeFromLeft (compKnobWidth);
    compThreshLabel.setBounds (compSlot.removeFromTop (20));
    compThreshLabel.setJustificationType (juce::Justification::centred);
    compThreshLabel.setFont (juce::FontOptions (14.0f));
    compThreshSlider.setBounds (compSlot.reduced (2));
    compStrip.removeFromLeft (knobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compRatioLabel.setBounds (compSlot.removeFromTop (20));
    compRatioLabel.setJustificationType (juce::Justification::centred);
    compRatioLabel.setFont (juce::FontOptions (14.0f));
    compRatioSlider.setBounds (compSlot.reduced (2));
    compStrip.removeFromLeft (knobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compAttackLabel.setBounds (compSlot.removeFromTop (20));
    compAttackLabel.setJustificationType (juce::Justification::centred);
    compAttackLabel.setFont (juce::FontOptions (14.0f));
    compAttackSlider.setBounds (compSlot.reduced (2));
    compStrip.removeFromLeft (knobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compReleaseLabel.setBounds (compSlot.removeFromTop (20));
    compReleaseLabel.setJustificationType (juce::Justification::centred);
    compReleaseLabel.setFont (juce::FontOptions (14.0f));
    compReleaseSlider.setBounds (compSlot.reduced (2));
    compStrip.removeFromLeft (knobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compMakeupLabel.setBounds (compSlot.removeFromTop (20));
    compMakeupLabel.setJustificationType (juce::Justification::centred);
    compMakeupLabel.setFont (juce::FontOptions (14.0f));
    compMakeupSlider.setBounds (compSlot.reduced (2));

    auto eqArea = eqSectionBounds.reduced (8);
    auto eqHeader = eqArea.removeFromTop (28);
    eqLabel.setBounds (eqHeader);
    eqLabel.setJustificationType (juce::Justification::centred);
    eqLabel.setFont (juce::FontOptions (16.0f));
    auto eqResetRow = eqArea.removeFromTop (26);
    resetButton.setBounds (eqResetRow.withSizeKeepingCentre (180, 24));
    eqVisualization.setBounds (eqArea.reduced (2));

    auto satArea = saturationSectionBounds.reduced (8);
    auto satHeaderArea = satArea.removeFromTop (28);
    saturationSectionLabel.setBounds (satHeaderArea);
    saturationSectionLabel.setJustificationType (juce::Justification::centred);
    saturationSectionLabel.setFont (juce::FontOptions (16.0f));

    const int satRows = 6;
    const int labelWidth = 70;
    const int rowHeight = satArea.getHeight() / satRows;
    const int satTotalWidth = labelWidth + knobWidth * 4 + knobGap * 4;

    auto layoutSatRow = [=] (juce::Rectangle<int> row,
                             juce::Label& groupLabel,
                             juce::Slider& a, juce::Label& aLabel,
                             juce::Slider& b, juce::Label& bLabel,
                             juce::Slider& c, juce::Label& cLabel,
                             juce::Slider& d, juce::Label& dLabel)
    {
        auto rowStrip = row.withWidth (satTotalWidth)
                           .withX (row.getX() + (row.getWidth() - satTotalWidth) / 2);
        auto labelArea = rowStrip.removeFromLeft (labelWidth);
        groupLabel.setBounds (labelArea.reduced (2));

        groupLabel.setJustificationType (juce::Justification::centred);
        groupLabel.setFont (juce::FontOptions (14.0f));
        
        auto knob = rowStrip.removeFromLeft (knobWidth);
        aLabel.setBounds (knob.removeFromTop (18));
        a.setBounds (knob.reduced (2));
        rowStrip.removeFromLeft (knobGap);

        knob = rowStrip.removeFromLeft (knobWidth);
        bLabel.setBounds (knob.removeFromTop (18));
        b.setBounds (knob.reduced (2));
        rowStrip.removeFromLeft (knobGap);

        knob = rowStrip.removeFromLeft (knobWidth);
        cLabel.setBounds (knob.removeFromTop (18));
        c.setBounds (knob.reduced (2));
        rowStrip.removeFromLeft (knobGap);

        knob = rowStrip.removeFromLeft (knobWidth);
        dLabel.setBounds (knob.removeFromTop (18));
        d.setBounds (knob.reduced (2));
    };

    saturationTypeBounds[0] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[0],
                  tubeLabel, tubeDriveSlider, tubeDriveLabel, tubeWarmthSlider, tubeWarmthLabel,
                  tubeBiasSlider, tubeBiasLabel, tubeOutputSlider, tubeOutputLabel);

    saturationTypeBounds[1] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[1],
                  transistorLabel, transistorDriveSlider, transistorDriveLabel, transistorBiteSlider, transistorBiteLabel,
                  transistorClipSlider, transistorClipLabel, transistorOutputSlider, transistorOutputLabel);

    saturationTypeBounds[2] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[2],
                  tapeLabel, tapeDriveSlider, tapeDriveLabel, tapeWowSlider, tapeWowLabel,
                  tapeHissSlider, tapeHissLabel, tapeOutputSlider, tapeOutputLabel);

    saturationTypeBounds[3] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[3],
                  diodeLabel, diodeDriveSlider, diodeDriveLabel, diodeAsymSlider, diodeAsymLabel,
                  diodeClipSlider, diodeClipLabel, diodeOutputSlider, diodeOutputLabel);

    saturationTypeBounds[4] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[4],
                  fuzzLabel, fuzzDriveSlider, fuzzDriveLabel, fuzzGateSlider, fuzzGateLabel,
                  fuzzToneSlider, fuzzToneLabel, fuzzOutputSlider, fuzzOutputLabel);

    saturationTypeBounds[5] = satArea.removeFromTop (rowHeight);
    layoutSatRow (saturationTypeBounds[5],
                  bitLabel, bitDepthSlider, bitDepthLabel, bitRateSlider, bitRateLabel,
                  bitMixSlider, bitMixLabel, bitOutputSlider, bitOutputLabel);
}

void StaticCurrentsPluginAudioProcessorEditor::updateRecordButton()
{
    if (audioProcessor.isRecording())
    {
        recordButton.setButtonText ("Stop Recording");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
        fileLabel.setText ("RECORDING...", juce::dontSendNotification);
    }
    else
    {
        recordButton.setButtonText ("Record");
        recordButton.setColour (juce::TextButton::buttonColourId,
                                getLookAndFeel().findColour (juce::TextButton::buttonColourId));

        if (fileLabel.getText() == "RECORDING...")
            fileLabel.setText ("Recording complete - sample loaded", juce::dontSendNotification);
    }
}

void StaticCurrentsPluginAudioProcessorEditor::updateEQVisualization()
{
    eqVisualization.setHPF (audioProcessor.getHPFFreqParameter()->load(),
                            audioProcessor.getHPFSlopeParameter()->load());
    eqVisualization.setParametricBand (1,
                                       audioProcessor.getPeak1FreqParameter()->load(),
                                       audioProcessor.getPeak1GainParameter()->load(),
                                       audioProcessor.getPeak1QParameter()->load());
    eqVisualization.setParametricBand (2,
                                       audioProcessor.getPeak2FreqParameter()->load(),
                                       audioProcessor.getPeak2GainParameter()->load(),
                                       audioProcessor.getPeak2QParameter()->load());
    eqVisualization.setParametricBand (3,
                                       audioProcessor.getPeak3FreqParameter()->load(),
                                       audioProcessor.getPeak3GainParameter()->load(),
                                       audioProcessor.getPeak3QParameter()->load());
    eqVisualization.setParametricBand (4,
                                       audioProcessor.getPeak4FreqParameter()->load(),
                                       audioProcessor.getPeak4GainParameter()->load(),
                                       audioProcessor.getPeak4QParameter()->load());
    eqVisualization.setLPF (audioProcessor.getLPFFreqParameter()->load(),
                            audioProcessor.getLPFSlopeParameter()->load());
}

void StaticCurrentsPluginAudioProcessorEditor::syncSlidersFromParameters()
{
    auto setSlider = [] (juce::Slider& slider, float value)
    {
        slider.setValue (value, juce::dontSendNotification);
    };

    setSlider (gainSlider, audioProcessor.getGainParameter()->load());
    setSlider (pitchSlider, audioProcessor.getPitchParameter()->load());

    setSlider (compThreshSlider, audioProcessor.getCompThreshParameter()->load());
    setSlider (compRatioSlider, audioProcessor.getCompRatioParameter()->load());
    setSlider (compAttackSlider, audioProcessor.getCompAttackParameter()->load());
    setSlider (compReleaseSlider, audioProcessor.getCompReleaseParameter()->load());
    setSlider (compMakeupSlider, audioProcessor.getCompMakeupParameter()->load());

    setSlider (tubeDriveSlider, audioProcessor.getTubeDriveParameter()->load());
    setSlider (tubeWarmthSlider, audioProcessor.getTubeWarmthParameter()->load());
    setSlider (tubeBiasSlider, audioProcessor.getTubeBiasParameter()->load());
    setSlider (tubeOutputSlider, audioProcessor.getTubeOutputParameter()->load());

    setSlider (transistorDriveSlider, audioProcessor.getTransistorDriveParameter()->load());
    setSlider (transistorBiteSlider, audioProcessor.getTransistorBiteParameter()->load());
    setSlider (transistorClipSlider, audioProcessor.getTransistorClipParameter()->load());
    setSlider (transistorOutputSlider, audioProcessor.getTransistorOutputParameter()->load());

    setSlider (tapeDriveSlider, audioProcessor.getTapeDriveParameter()->load());
    setSlider (tapeWowSlider, audioProcessor.getTapeWowParameter()->load());
    setSlider (tapeHissSlider, audioProcessor.getTapeHissParameter()->load());
    setSlider (tapeOutputSlider, audioProcessor.getTapeOutputParameter()->load());

    setSlider (diodeDriveSlider, audioProcessor.getDiodeDriveParameter()->load());
    setSlider (diodeAsymSlider, audioProcessor.getDiodeAsymParameter()->load());
    setSlider (diodeClipSlider, audioProcessor.getDiodeClipParameter()->load());
    setSlider (diodeOutputSlider, audioProcessor.getDiodeOutputParameter()->load());

    setSlider (fuzzDriveSlider, audioProcessor.getFuzzDriveParameter()->load());
    setSlider (fuzzGateSlider, audioProcessor.getFuzzGateParameter()->load());
    setSlider (fuzzToneSlider, audioProcessor.getFuzzToneParameter()->load());
    setSlider (fuzzOutputSlider, audioProcessor.getFuzzOutputParameter()->load());

    setSlider (bitDepthSlider, audioProcessor.getBitDepthParameter()->load());
    setSlider (bitRateSlider, audioProcessor.getBitRateParameter()->load());
    setSlider (bitMixSlider, audioProcessor.getBitMixParameter()->load());
    setSlider (bitOutputSlider, audioProcessor.getBitOutputParameter()->load());
}
