/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#if defined(_WIN32)
#include <sapi.h>  // Windows Speech API
#elif defined(__APPLE__)
#include <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#endif

//==============================================================================
StaticCurrentsPluginAudioProcessorEditor::StaticCurrentsPluginAudioProcessorEditor (StaticCurrentsPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Check if this is the effect version
    isEffect = isEffectVersion();
    
    // Load image from Resources folder - try multiple potential locations
    {
        juce::File imageFile;
        
        // Try project root Resources folder
        imageFile = juce::File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("SCimage.png");
        
        if (!imageFile.existsAsFile())
            imageFile = juce::File::getCurrentWorkingDirectory().getChildFile("Resources").getChildFile("SCimage.png");
        
        if (!imageFile.existsAsFile())
            imageFile = juce::File("/Users/karterbrown/Desktop/Dev/Plugins/StaticCurrentsPlugin/Resources/SCimage.png");
        
        if (imageFile.existsAsFile())
            logoImage = juce::ImageCache::getFromFile(imageFile);
    }
    
    // Setup load button (instrument/standalone only)
    if (!isEffect) {
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
    }  // Close if (!isEffect) for loadButton

    addAndMakeVisible (fileLabel);
    if (!isEffect) {
        fileLabel.setText ("No sample loaded", juce::dontSendNotification);
    } else {
        fileLabel.setVisible(false);
    }
    fileLabel.setJustificationType (juce::Justification::centred);

    // Setup record button (instrument/standalone only)
    if (!isEffect) {
        addAndMakeVisible (recordButton);
    recordButton.onClick = [this]
    {
        if (audioProcessor.isRecording())
        {
            audioProcessor.stopRecording();
            if (audioProcessor.hasLoadedSample())
                fileLabel.setText ("Recorded sample loaded", juce::dontSendNotification);
            else
                fileLabel.setText ("No sample recorded", juce::dontSendNotification);
        }
        else
        {
            audioProcessor.stopSamplePlayback();
            isPlaying = false;
            playButton.setButtonText ("Play Sample");
            playButton.setColour (juce::TextButton::buttonColourId,
                                  getLookAndFeel().findColour (juce::TextButton::buttonColourId));
            audioProcessor.startRecording();
            fileLabel.setText ("Recording...", juce::dontSendNotification);
        }
        updateRecordButton();
    };
    }  // Close if (!isEffect) for recordButton

    // Setup bypass button (instrument/standalone only)
    if (!isEffect) {
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
    }

    // Setup play button (instrument/standalone only)
    if (!isEffect) {
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
            playButton.setButtonText ("Stop");
            playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::green);
            isPlaying = true;
        }
    };
    }  // Close if (!isEffect) for playButton

    // Setup TTS text editor (instrument/standalone only)
    if (!isEffect) {
        addAndMakeVisible (ttsTextEditor);
    ttsTextEditor.setMultiLine (true);
    ttsTextEditor.setReturnKeyStartsNewLine (true);
    ttsTextEditor.setScrollbarsShown (true);
    ttsTextEditor.setCaretVisible (true);
    ttsTextEditor.setPopupMenuEnabled (true);
    ttsTextEditor.setText ("Enter text here to convert to speech...");
    
    addAndMakeVisible (ttsLabel);

    // Setup TTS generate button (text-to-speech)
    addAndMakeVisible (generateButton);
    generateButton.onClick = [this]
    {
        // Get text from editor
        juce::String text = ttsTextEditor.getText();
        if (text.isEmpty() || text == "Enter text here to convert to speech...")
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                     "No Text",
                                                     "Please enter some text to convert to speech.");
            return;
        }

#if defined(_WIN32)
        // Use Windows SAPI for text-to-speech
        CoInitialize (nullptr);
        ISpVoice* pVoice = nullptr;

        if (SUCCEEDED (CoCreateInstance (CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice)))
        {
            // Create a temporary WAV file
            juce::File tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                       .getChildFile ("tts_temp.wav");

            // Setup file stream
            ISpStream* pStream = nullptr;
            if (SUCCEEDED (CoCreateInstance (CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, (void**)&pStream)))
            {
                // Setup wave format manually (44kHz, 16-bit, mono)
                WAVEFORMATEX wfex;
                wfex.wFormatTag = WAVE_FORMAT_PCM;
                wfex.nChannels = 1;
                wfex.nSamplesPerSec = 44100;
                wfex.wBitsPerSample = 16;
                wfex.nBlockAlign = (wfex.nChannels * wfex.wBitsPerSample) / 8;
                wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
                wfex.cbSize = 0;

                GUID formatGuid = SPDFID_WaveFormatEx;

                if (SUCCEEDED (pStream->BindToFile (tempFile.getFullPathName().toWideCharPointer(),
                                                     SPFM_CREATE_ALWAYS,
                                                     &formatGuid,
                                                     &wfex,
                                                     SPFEI_ALL_EVENTS)))
                {
                    pVoice->SetOutput (pStream, TRUE);
                    pVoice->Speak (text.toWideCharPointer(), SPF_DEFAULT, nullptr);

                    pStream->Release();

                    // Load the generated WAV file into the sampler
                    juce::MessageManager::callAsync ([this, tempFile]()
                    {
                        if (tempFile.existsAsFile())
                        {
                            audioProcessor.loadSampleFromFile (tempFile);
                            fileLabel.setText (tempFile.getFileNameWithoutExtension(), juce::dontSendNotification);

                            // Clean up temp file after a delay
                            juce::Timer::callAfterDelay (1000, [tempFile]()
                            {
                                tempFile.deleteFile();
                            });
                        }
                    });
                }

            pVoice->Release();
        }

        CoUninitialize();
        updateRecordButton();
#elif defined(__APPLE__)
        // Use macOS NSSpeechSynthesizer for text-to-speech
        @autoreleasepool {
            NSSpeechSynthesizer* synth = [[NSSpeechSynthesizer alloc] initWithVoice:nil];
            
            if (synth != nil)
            {
                // Create a temporary AIFF file (NSSpeechSynthesizer outputs AIFF)
                juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                           .getChildFile("tts_temp.aiff");
                
                NSString* nsText = [NSString stringWithUTF8String:text.toRawUTF8()];
                NSURL* outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:tempFile.getFullPathName().toRawUTF8()]];
                
                // Start speaking to file
                BOOL success = [synth startSpeakingString:nsText toURL:outputURL];
                
                if (success)
                {
                    // Wait for synthesis to complete (NSSpeechSynthesizer is asynchronous)
                    while ([synth isSpeaking])
                    {
                        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
                    }
                    
                    // Convert AIFF to WAV for consistency
                    juce::File wavFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                              .getChildFile("tts_temp.wav");
                    
                    // Load AIFF and convert to WAV
                    juce::AudioFormatManager formatManager;
                    formatManager.registerBasicFormats();
                    
                    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(tempFile));
                    if (reader != nullptr)
                    {
                        juce::WavAudioFormat wavFormat;
                        std::unique_ptr<juce::FileOutputStream> outStream(wavFile.createOutputStream());
                        
                        if (outStream != nullptr)
                        {
                            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                                outStream.get(),
                                44100.0,
                                reader->numChannels,
                                16,
                                {},
                                0));
                            
                            if (writer != nullptr)
                            {
                                outStream.release(); // writer now owns the stream
                                writer->writeFromAudioReader(*reader, 0, reader->lengthInSamples);
                                writer.reset();
                                
                                // Load the WAV file into the sampler
                                juce::MessageManager::callAsync([this, wavFile, tempFile]()
                                {
                                    if (wavFile.existsAsFile())
                                    {
                                        audioProcessor.loadSampleFromFile(wavFile);
                                        fileLabel.setText("TTS: Generated", juce::dontSendNotification);
                                        
                                        // Clean up temp files after a delay
                                        juce::Timer::callAfterDelay(1000, [wavFile, tempFile]()
                                        {
                                            wavFile.deleteFile();
                                            tempFile.deleteFile();
                                        });
                                    }
                                });
                            }
                        }
                    }
                    
                    // Clean up temp AIFF if WAV conversion failed
                    if (!wavFile.existsAsFile())
                    {
                        // Just load the AIFF directly
                        juce::MessageManager::callAsync([this, tempFile]()
                        {
                            if (tempFile.existsAsFile())
                            {
                                audioProcessor.loadSampleFromFile(tempFile);
                                fileLabel.setText("TTS: Generated", juce::dontSendNotification);
                                
                                juce::Timer::callAfterDelay(1000, [tempFile]()
                                {
                                    tempFile.deleteFile();
                                });
                            }
                        });
                    }
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                           "Text-to-Speech",
                                                           "Failed to generate speech.");
                }
            }
        }
        updateRecordButton();
#else
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                 "Text-to-Speech",
                                                 "Text-to-speech is only available on Windows and macOS.");
#endif
    };
    }  // Close if (!isEffect) for TTS

    // Setup jumble button (instrument/standalone only)
    if (!isEffect) {
        addAndMakeVisible (jumbleButton);
        jumbleButton.onClick = [this]
        {
            audioProcessor.jumbleSample();
            updateRecordButton();
        };

        // Setup export button (formerly generate - export to file)
        addAndMakeVisible (exportButton);
    exportButton.onClick = [this]
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
    }  // Close if (!isEffect) for export/jumble

    // Setup progress slider (instrument/standalone only)
    if (!isEffect) {
        addAndMakeVisible (progressSlider);
        addAndMakeVisible (progressLabel);
    progressSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    progressSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setRange (0.0, 1.0, 0.001);
    progressLabel.setJustificationType (juce::Justification::centred);
    progressSlider.setValue (0.0);
    progressSlider.onDragStart = [this]
    {
        if (isPlaying)
            audioProcessor.stopSamplePlayback();
    };
        // Disable all progress slider interactions to prevent unintended playback
        progressSlider.onValueChange = nullptr;
        progressSlider.onDragEnd = nullptr;
    }  // Close if (!isEffect) for progressSlider

    // Basic parameters
    addAndMakeVisible (gainSectionLabel);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (gainLabel);
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    gainSlider.setRange (0.0, 1.0, 0.01);
    gainSlider.setValue (0.7);
    gainSlider.onValueChange = [this] { *audioProcessor.getGainParameter() = static_cast<float> (gainSlider.getValue()); };

    addAndMakeVisible (pitchSlider);
    addAndMakeVisible (pitchLabel);
    pitchSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    pitchSlider.setRange (0.5, 2.0, 0.01);
    pitchSlider.setValue (1.0);
    pitchSlider.setTextValueSuffix (" x");
    pitchSlider.onValueChange = [this] { *audioProcessor.getPitchParameter() = static_cast<float> (pitchSlider.getValue()); };

    // Global Output Slider
    addAndMakeVisible (globalOutputSlider);
    addAndMakeVisible (globalOutputLabel);
    globalOutputSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    globalOutputSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    globalOutputSlider.setRange (-24.0, 6.0, 0.1);
    globalOutputSlider.setValue (0.0);
    globalOutputSlider.setTextValueSuffix (" dB");
    globalOutputSlider.onValueChange = [this] { *audioProcessor.getGlobalOutputParameter() = static_cast<float> (globalOutputSlider.getValue()); };

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
        
        // Reload the original recorded sample if it exists
        if (audioProcessor.getOriginalRecordingFile().existsAsFile())
        {
            audioProcessor.loadSampleFromFile(audioProcessor.getOriginalRecordingFile());
            DBG("Reset: Reloaded original recording");
        }
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
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider.setRange (min, max, step);
        slider.setValue (value);
        slider.onValueChange = [this, param, &slider]
        {
            *param = static_cast<float> (slider.getValue());
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

    compThreshSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    compThreshSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    compThreshSlider.setRange (-60.0, 0.0, 0.1);
    compThreshSlider.setValue (-20.0);
    compThreshSlider.setTextValueSuffix (" dB");
    compThreshSlider.onValueChange = [this] { *audioProcessor.getCompThreshParameter() = static_cast<float> (compThreshSlider.getValue()); };

    compRatioSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    compRatioSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    compRatioSlider.setRange (1.0, 20.0, 0.1);
    compRatioSlider.setValue (4.0);
    compRatioSlider.setTextValueSuffix (":1");
    compRatioSlider.onValueChange = [this] { *audioProcessor.getCompRatioParameter() = static_cast<float> (compRatioSlider.getValue()); };

    compAttackSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    compAttackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    compAttackSlider.setRange (0.001, 0.1, 0.001);
    compAttackSlider.setValue (0.01);
    compAttackSlider.setSkewFactorFromMidPoint (0.01);
    compAttackSlider.setTextValueSuffix (" s");
    compAttackSlider.onValueChange = [this] { *audioProcessor.getCompAttackParameter() = static_cast<float> (compAttackSlider.getValue()); };

    compReleaseSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    compReleaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    compReleaseSlider.setRange (0.01, 1.0, 0.01);
    compReleaseSlider.setValue (0.1);
    compReleaseSlider.setSkewFactorFromMidPoint (0.1);
    compReleaseSlider.setTextValueSuffix (" s");
    compReleaseSlider.onValueChange = [this] { *audioProcessor.getCompReleaseParameter() = static_cast<float> (compReleaseSlider.getValue()); };

    compMakeupSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    compMakeupSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
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
    addClickReset(globalOutputSlider, 0.0);

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

    const int minWidth = 1000;
    const int minHeight = 700;

    // Start at minimum size
    auto displays = juce::Desktop::getInstance().getDisplays();
    auto primaryDisplay = displays.getPrimaryDisplay();
    if (primaryDisplay != nullptr)
    {
        auto screenArea = primaryDisplay->userArea;
        setSize (juce::jmin (minWidth, screenArea.getWidth()),
                 juce::jmin (minHeight, screenArea.getHeight()));
        setResizeLimits (juce::jmin (minWidth, screenArea.getWidth()),
                         juce::jmin (minHeight, screenArea.getHeight()),
                         4800,
                         3600);
    }
    else
    {
        setSize (minWidth, minHeight);
        setResizeLimits (minWidth, minHeight, 4800, 3600);
    }

    setResizable (true, true);
    
    // For standalone apps, make sure the resize constraints are properly set
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setMinimumSize(juce::jmin(minWidth, 1500), juce::jmin(minHeight, 950));
        constrainer->setMaximumSize(4800, 3600);
    }
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
        playButton.setEnabled (true);
        progressSlider.setEnabled (true);
        progressSlider.setRange (0.0, length, 0.01);

        if (!progressSlider.isMouseButtonDown())
        {
            float position = audioProcessor.getPlaybackPosition();
            progressSlider.setValue (position, juce::dontSendNotification);
            
            // Check if playback has finished
            if (isPlaying && !audioProcessor.isCurrentlyPlaying())
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

        progressLabel.setText (juce::String::formatted ("%d:%02d",
                                                        currentMin, currentSec),
                              juce::dontSendNotification);
    }
    else
    {
        progressSlider.setEnabled (false);
        progressSlider.setValue (0.0, juce::dontSendNotification);
        progressLabel.setText ("0:00", juce::dontSendNotification);
        playButton.setEnabled (false);
        isPlaying = false;
        playButton.setButtonText ("Play Sample");
        playButton.setColour (juce::TextButton::buttonColourId,
                              getLookAndFeel().findColour (juce::TextButton::buttonColourId));
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
    if (isEffect) {
        g.drawFittedText ("Audio Processor", getLocalBounds(), juce::Justification::centredTop, 1);
    } else {
        g.drawFittedText ("Voice Sampler & Processor", getLocalBounds(), juce::Justification::centredTop, 1);
    }
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

    auto bounds = getLocalBounds().reduced (6);

    // Compact top section - 3 columns: Left controls | Middle (Logo) | Right controls
    const int topSectionHeight = isEffect ? 115 : 175;
    auto topArea = bounds.removeFromTop (topSectionHeight);
    topSectionBounds = topArea;
    
    const int colGap = 8;
    const int leftColWidth = (int)(topArea.getWidth() * 0.30);
    const int rightColWidth = (int)(topArea.getWidth() * 0.30);
    
    auto leftCol = topArea.removeFromLeft (leftColWidth);
    topArea.removeFromLeft (colGap);
    auto rightCol = topArea.removeFromRight (rightColWidth);
    topArea.removeFromRight (colGap);
    auto middleCol = topArea;  // Logo area
    
    // Left column: TTS + Progress
    if (!isEffect) {
        auto ttsArea = leftCol.removeFromTop (55);
        ttsLabel.setBounds (ttsArea.removeFromTop (16));
        ttsTextEditor.setBounds (ttsArea.reduced (2));
        
        leftCol.removeFromTop (4);
        auto progressArea = leftCol.removeFromTop (30);
        auto progLabel = progressArea.removeFromLeft (65);
        progressLabel.setBounds (progLabel);
        progressArea.removeFromLeft (4);
        progressSlider.setBounds (progressArea);
        
        leftCol.removeFromTop (4);
        auto profileArea = leftCol.removeFromTop (22);
        auto profLabel = profileArea.removeFromLeft (65);
        profileLabel.setBounds (profLabel);
        profileArea.removeFromLeft (4);
        profileBox.setBounds (profileArea);
    }
    
    // Middle column: Logo image
    logoImageBounds = middleCol.reduced (10);
    
    // Right column: Buttons + File label
    if (!isEffect) {
        const int buttonHeight = 28;
        const int buttonGap = 3;
        auto btnArea = rightCol;
        
        // Stack buttons in 2 columns
        const int btnColWidth = (btnArea.getWidth() - buttonGap) / 2;
        auto leftBtnCol = btnArea.removeFromLeft (btnColWidth);
        btnArea.removeFromLeft (buttonGap);
        auto rightBtnCol = btnArea;
        
        loadButton.setBounds (leftBtnCol.removeFromTop (buttonHeight));
        leftBtnCol.removeFromTop (buttonGap);
        recordButton.setBounds (leftBtnCol.removeFromTop (buttonHeight));
        leftBtnCol.removeFromTop (buttonGap);
        bypassButton.setBounds (leftBtnCol.removeFromTop (buttonHeight));
        leftBtnCol.removeFromTop (buttonGap);
        playButton.setBounds (leftBtnCol.removeFromTop (buttonHeight));
        
        generateButton.setBounds (rightBtnCol.removeFromTop (buttonHeight));
        rightBtnCol.removeFromTop (buttonGap);
        jumbleButton.setBounds (rightBtnCol.removeFromTop (buttonHeight));
        rightBtnCol.removeFromTop (buttonGap);
        exportButton.setBounds (rightBtnCol.removeFromTop (buttonHeight));
        rightBtnCol.removeFromTop (buttonGap);
        fileLabel.setBounds (rightBtnCol.removeFromTop (buttonHeight));
    }

    auto content = bounds;
    bounds.removeFromTop (4);
    
    // Signal flow layout: Top row = Input + EQ + Dynamics, Bottom row = Saturation (full width)
    const int rowGap = 6;
    const int topRowHeight = juce::jmax(140, (int)(content.getHeight() * 0.35));  // Top gets ~35% of height
    
    // Top row: Input section, EQ section, Dynamics section
    auto topRow = content.removeFromTop (topRowHeight);
    content.removeFromTop (rowGap);
    
    const int columnGap = 8;
    const int inputColWidth = (int)(topRow.getWidth() * 0.13);  // Input gets ~13% width
    const int dynamicsColWidth = (int)(topRow.getWidth() * 0.20);  // Dynamics gets ~20% width
    
    gainSectionBounds = topRow.removeFromLeft (inputColWidth);
    topRow.removeFromLeft (columnGap);
    
    // Calculate remaining width for EQ and Dynamics
    auto dynCol = topRow.removeFromRight (dynamicsColWidth);
    topRow.removeFromRight (columnGap);
    eqSectionBounds = topRow;  // EQ gets middle space
    compSectionBounds = dynCol;
    
    // Bottom row: Saturation (full width) - now gets ~65% of height
    saturationSectionBounds = content;

    // Uniform knob size throughout the entire UI - smallest size
    const int knobWidth = 65;
    const int knobGap = 5;

    // Gain section - vertically stacked Gain and Pitch knobs
    auto gainArea = gainSectionBounds.reduced (3);
    auto gainHeader = gainArea.removeFromTop (16);
    gainSectionLabel.setBounds (gainHeader);
    gainSectionLabel.setJustificationType (juce::Justification::centred);
    gainSectionLabel.setFont (juce::FontOptions (10.0f));
    
    const int gainKnobHeight = (gainArea.getHeight() - knobGap) / 2;
    auto knobSlot = gainArea.removeFromTop (gainKnobHeight);
    gainLabel.setBounds (knobSlot.removeFromTop (12));
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setFont (juce::FontOptions (9.0f));
    gainSlider.setBounds (knobSlot.reduced (8, 0));
    
    gainArea.removeFromTop (knobGap);
    knobSlot = gainArea.removeFromTop (gainKnobHeight);
    pitchLabel.setBounds (knobSlot.removeFromTop (12));
    pitchLabel.setJustificationType (juce::Justification::centred);
    pitchLabel.setFont (juce::FontOptions (9.0f));
    pitchSlider.setBounds (knobSlot.reduced (8, 0));

    // Compression section - wrap knobs in 2 rows for better fit
    auto compArea = compSectionBounds.reduced (3);
    auto compHeaderArea = compArea.removeFromTop (16);
    compLabel.setBounds (compHeaderArea);
    compLabel.setJustificationType (juce::Justification::centred);
    compLabel.setFont (juce::FontOptions (10.0f));
    
    // Reserve space for Output knob at bottom
    auto outputArea = compArea.removeFromBottom (70);
    compArea.removeFromBottom (3);  // Small gap
    
    // Split compressor knobs into 2 rows: top row = 3 knobs, bottom row = 2 knobs
    const int compRowHeight = (compArea.getHeight() - knobGap) / 2;
    const int compKnobWidth = 58;
    const int compKnobGap = 4;
    
    // Top row: Threshold, Ratio, Attack
    auto compTopRow = compArea.removeFromTop (compRowHeight);
    const int topRowWidth = compKnobWidth * 3 + compKnobGap * 2;
    auto topStrip = compTopRow.withWidth (topRowWidth)
                          .withX (compTopRow.getX() + (compTopRow.getWidth() - topRowWidth) / 2);
    
    compArea.removeFromTop (knobGap);
    
    // Bottom row: Release, Makeup
    auto compBottomRow = compArea;
    const int bottomRowWidth = compKnobWidth * 2 + compKnobGap;
    auto bottomStrip = compBottomRow.withWidth (bottomRowWidth)
                                .withX (compBottomRow.getX() + (compBottomRow.getWidth() - bottomRowWidth) / 2);
    
    auto compStrip = topStrip;
    auto compSlot = compStrip.removeFromLeft (compKnobWidth);
    compThreshLabel.setBounds (compSlot.removeFromTop (12));
    compThreshLabel.setJustificationType (juce::Justification::centred);
    compThreshLabel.setFont (juce::FontOptions (8.0f));
    compThreshSlider.setBounds (compSlot);
    compStrip.removeFromLeft (compKnobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compRatioLabel.setBounds (compSlot.removeFromTop (12));
    compRatioLabel.setJustificationType (juce::Justification::centred);
    compRatioLabel.setFont (juce::FontOptions (8.0f));
    compRatioSlider.setBounds (compSlot);
    compStrip.removeFromLeft (compKnobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compAttackLabel.setBounds (compSlot.removeFromTop (12));
    compAttackLabel.setJustificationType (juce::Justification::centred);
    compAttackLabel.setFont (juce::FontOptions (8.0f));
    compAttackSlider.setBounds (compSlot);

    // Bottom row
    compStrip = bottomStrip;
    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compReleaseLabel.setBounds (compSlot.removeFromTop (12));
    compReleaseLabel.setJustificationType (juce::Justification::centred);
    compReleaseLabel.setFont (juce::FontOptions (8.0f));
    compReleaseSlider.setBounds (compSlot);
    compStrip.removeFromLeft (compKnobGap);

    compSlot = compStrip.removeFromLeft (compKnobWidth);
    compMakeupLabel.setBounds (compSlot.removeFromTop (12));
    compMakeupLabel.setJustificationType (juce::Justification::centred);
    compMakeupLabel.setFont (juce::FontOptions (8.0f));
    compMakeupSlider.setBounds (compSlot);
    
    // Output knob at bottom of compression section
    globalOutputLabel.setBounds (outputArea.removeFromTop (11));
    globalOutputLabel.setJustificationType (juce::Justification::centred);
    globalOutputLabel.setFont (juce::FontOptions (8.5f));
    globalOutputSlider.setBounds (outputArea.reduced (6, 0));

    auto eqArea = eqSectionBounds.reduced (3);
    auto eqHeader = eqArea.removeFromTop (16);
    eqLabel.setBounds (eqHeader);
    eqLabel.setJustificationType (juce::Justification::centred);
    eqLabel.setFont (juce::FontOptions (10.0f));
    auto eqResetRow = eqArea.removeFromTop (22);
    resetButton.setBounds (eqResetRow.withSizeKeepingCentre (150, 20));
    eqVisualization.setBounds (eqArea.reduced (2));

    auto satArea = saturationSectionBounds.reduced (3);
    auto satHeaderArea = satArea.removeFromTop (20);
    saturationSectionLabel.setBounds (satHeaderArea);
    saturationSectionLabel.setJustificationType (juce::Justification::centred);
    saturationSectionLabel.setFont (juce::FontOptions (12.0f));

    const int satRows = 6;
    // Saturation is now the main section - larger knobs and labels
    const int labelWidth = 75;
    const int satKnobWidth = 78;
    const int satKnobGap = 8;
    const int rowHeight = satArea.getHeight() / satRows;
    const int satTotalWidth = labelWidth + satKnobWidth * 4 + satKnobGap * 3;

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
        groupLabel.setFont (juce::FontOptions (11.0f));
        
        auto knob = rowStrip.removeFromLeft (satKnobWidth);
        aLabel.setBounds (knob.removeFromTop (14));
        aLabel.setJustificationType (juce::Justification::centred);
        aLabel.setFont (juce::FontOptions (10.0f));
        a.setBounds (knob);
        rowStrip.removeFromLeft (satKnobGap);

        knob = rowStrip.removeFromLeft (satKnobWidth);
        bLabel.setBounds (knob.removeFromTop (14));
        bLabel.setJustificationType (juce::Justification::centred);
        bLabel.setFont (juce::FontOptions (10.0f));
        b.setBounds (knob);
        rowStrip.removeFromLeft (satKnobGap);

        knob = rowStrip.removeFromLeft (satKnobWidth);
        cLabel.setBounds (knob.removeFromTop (14));
        cLabel.setJustificationType (juce::Justification::centred);
        cLabel.setFont (juce::FontOptions (10.0f));
        c.setBounds (knob);
        rowStrip.removeFromLeft (satKnobGap);

        knob = rowStrip.removeFromLeft (satKnobWidth);
        dLabel.setBounds (knob.removeFromTop (14));
        dLabel.setJustificationType (juce::Justification::centred);
        dLabel.setFont (juce::FontOptions (10.0f));
        d.setBounds (knob);
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
    setSlider (globalOutputSlider, audioProcessor.getGlobalOutputParameter()->load());

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

bool StaticCurrentsPluginAudioProcessorEditor::isEffectVersion() const
{
    // Check the executable/bundle path to determine if this is the effect version
    auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto bundlePath = exePath.getParentDirectory().getParentDirectory();
    
    // Check if "Effect" appears in the bundle name or path
    return bundlePath.getFullPathName().contains("Effect") || 
           exePath.getFullPathName().contains("Effect");
}
