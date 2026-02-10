/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EQComponent.h"

//==============================================================================
// Helper class for click-to-reset functionality
class ClickToResetListener : public juce::MouseListener
{
public:
    ClickToResetListener(juce::Slider& slider, double defaultValue)
        : sliderRef(slider), defaultVal(defaultValue) {}
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        mouseDownPos = event.getPosition();
    }
    
    void mouseUp(const juce::MouseEvent& event) override
    {
        auto mouseUpPos = event.getPosition();
        auto distance = mouseDownPos.getDistanceFrom(mouseUpPos);
        
        // If mouse moved less than 3 pixels, treat it as a click (not a drag)
        if (distance < 3.0f)
        {
            sliderRef.setValue(defaultVal, juce::sendNotificationSync);
        }
    }
    
private:
    juce::Slider& sliderRef;
    double defaultVal;
    juce::Point<int> mouseDownPos;
};

//==============================================================================
/**
*/
class StaticCurrentsPluginAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    StaticCurrentsPluginAudioProcessorEditor (StaticCurrentsPluginAudioProcessor&);
    ~StaticCurrentsPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateRecordButton();
    void updateEQVisualization();
  void syncSlidersFromParameters();
    
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    StaticCurrentsPluginAudioProcessor& audioProcessor;

    // Buttons
    juce::TextButton loadButton { "Load Sample" };
    juce::TextButton recordButton { "Record" };
    juce::TextButton bypassButton { "Bypass" };
    juce::TextButton playButton { "Play Sample" };
    juce::TextButton generateButton { "Generate" };  // TTS
    juce::TextButton jumbleButton { "Jumble" };
    juce::TextButton exportButton { "Export" };  // Export to file
    juce::Label fileLabel;
    
    // Text-to-speech text input
    juce::TextEditor ttsTextEditor;
    juce::Label ttsLabel { {}, "Text to Speech:" };
    
    juce::Slider progressSlider;
    juce::Label progressLabel { {}, "0:00" };
    juce::Image logoImage;
    juce::Rectangle<int> logoImageBounds;
    
    bool isPlaying = false;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Basic parameters
    juce::Slider gainSlider, pitchSlider, globalOutputSlider;
    juce::Label gainLabel { {}, "Gain" }, pitchLabel { {}, "Pitch" }, globalOutputLabel { {}, "Output" };
    juce::ComboBox profileBox;
      bool profileInitialized = false;
    juce::Label profileLabel { {}, "Profile" };
    
    
    // 6-Band Parametric EQ
    EQComponent eqVisualization;
    juce::Label eqLabel { {}, "6-Band Parametric EQ" };
    juce::TextButton resetButton { "Reset All Parameters" };
    juce::Label eqParamsLabel { {}, "EQ" };

    // Saturation controls (replace EQ sliders section)
    juce::Label saturationSectionLabel { {}, "Saturation" };

    juce::Label tubeLabel { {}, "Tube" };
    juce::Slider tubeDriveSlider, tubeWarmthSlider, tubeBiasSlider, tubeOutputSlider;
    juce::Label tubeDriveLabel { {}, "Drive" }, tubeWarmthLabel { {}, "Warmth" };
    juce::Label tubeBiasLabel { {}, "Bias" }, tubeOutputLabel { {}, "Out" };

    juce::Label transistorLabel { {}, "Transistor" };
    juce::Slider transistorDriveSlider, transistorBiteSlider, transistorClipSlider, transistorOutputSlider;
    juce::Label transistorDriveLabel { {}, "Drive" }, transistorBiteLabel { {}, "Bite" };
    juce::Label transistorClipLabel { {}, "Clip" }, transistorOutputLabel { {}, "Out" };

    juce::Label tapeLabel { {}, "Tape" };
    juce::Slider tapeDriveSlider, tapeWowSlider, tapeHissSlider, tapeOutputSlider;
    juce::Label tapeDriveLabel { {}, "Drive" }, tapeWowLabel { {}, "Wow" };
    juce::Label tapeHissLabel { {}, "Hiss" }, tapeOutputLabel { {}, "Out" };

    juce::Label diodeLabel { {}, "Diode" };
    juce::Slider diodeDriveSlider, diodeAsymSlider, diodeClipSlider, diodeOutputSlider;
    juce::Label diodeDriveLabel { {}, "Drive" }, diodeAsymLabel { {}, "Asym" };
    juce::Label diodeClipLabel { {}, "Clip" }, diodeOutputLabel { {}, "Out" };

    juce::Label fuzzLabel { {}, "Fuzz" };
    juce::Slider fuzzDriveSlider, fuzzGateSlider, fuzzToneSlider, fuzzOutputSlider;
    juce::Label fuzzDriveLabel { {}, "Drive" }, fuzzGateLabel { {}, "Gate" };
    juce::Label fuzzToneLabel { {}, "Tone" }, fuzzOutputLabel { {}, "Out" };

    juce::Label bitLabel { {}, "Bitcrush" };
    juce::Slider bitDepthSlider, bitRateSlider, bitMixSlider, bitOutputSlider;
    juce::Label bitDepthLabel { {}, "Bits" }, bitRateLabel { {}, "Rate" };
    juce::Label bitMixLabel { {}, "Mix" }, bitOutputLabel { {}, "Out" };
    
    juce::Slider hpfFreqSlider, hpfSlopeSlider;
    juce::Label hpfLabel { {}, "HPF" };
    juce::Label hpfFreqLabel { {}, "Freq" }, hpfSlopeLabel { {}, "Slope" };
    
    juce::Slider peak1FreqSlider, peak1GainSlider, peak1QSlider;
    juce::Slider peak2FreqSlider, peak2GainSlider, peak2QSlider;
    juce::Slider peak3FreqSlider, peak3GainSlider, peak3QSlider;
    juce::Slider peak4FreqSlider, peak4GainSlider, peak4QSlider;
    juce::Label peak1Label { {}, "Peak 1" }, peak2Label { {}, "Peak 2" };
    juce::Label peak3Label { {}, "Peak 3" }, peak4Label { {}, "Peak 4" };
    juce::Label peak1FreqLabel { {}, "Freq" }, peak1GainLabel { {}, "Gain" }, peak1QLabel { {}, "Q" };
    juce::Label peak2FreqLabel { {}, "Freq" }, peak2GainLabel { {}, "Gain" }, peak2QLabel { {}, "Q" };
    juce::Label peak3FreqLabel { {}, "Freq" }, peak3GainLabel { {}, "Gain" }, peak3QLabel { {}, "Q" };
    juce::Label peak4FreqLabel { {}, "Freq" }, peak4GainLabel { {}, "Gain" }, peak4QLabel { {}, "Q" };
    
    juce::Slider lpfFreqSlider, lpfSlopeSlider;
    juce::Label lpfLabel { {}, "LPF" };
    juce::Label lpfFreqLabel { {}, "Freq" }, lpfSlopeLabel { {}, "Slope" };
    
    // Compressor
    juce::Slider compThreshSlider, compRatioSlider, compAttackSlider, compReleaseSlider, compMakeupSlider;
    juce::Label compLabel { {}, "FET Compressor" };
    juce::Label compThreshLabel { {}, "Thresh" }, compRatioLabel { {}, "Ratio" };
    juce::Label compAttackLabel { {}, "Attack" }, compReleaseLabel { {}, "Release" }, compMakeupLabel { {}, "Makeup" };

    juce::Rectangle<int> topSectionBounds;
    juce::Rectangle<int> eqSectionBounds;
    juce::Rectangle<int> gainSectionBounds;
    juce::Rectangle<int> compSectionBounds;
    juce::Rectangle<int> saturationSectionBounds;
    juce::Rectangle<int> saturationTypeBounds[6];

    // Click-to-reset listeners for all sliders
    std::vector<std::unique_ptr<ClickToResetListener>> clickResetListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaticCurrentsPluginAudioProcessorEditor)
};
