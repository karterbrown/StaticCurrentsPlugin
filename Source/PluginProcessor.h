/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TubeSaturation.h"

//==============================================================================
/**
*/
class StaticCurrentsPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    StaticCurrentsPluginAudioProcessor();
    ~StaticCurrentsPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Sampler functionality
    void loadSampleFromFile (const juce::File& file);
    
    // Recording functionality
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recording; }
    bool hasLoadedSample() const { return sampler.getNumSounds() > 0; }
    void triggerSamplePlayback() { shouldTriggerNote.store(true); }
    void stopSamplePlayback() { shouldStopNote.store(true); }
    void setLoopPlayback(bool shouldLoop) { loopPlayback.store(shouldLoop); }
    bool isLoopPlaybackEnabled() const { return loopPlayback.load(); }
    bool isCurrentlyPlaying() const { return isNoteCurrentlyPlaying; }
    void seekToPosition(float positionInSeconds) { seekPosition.store(positionInSeconds); }
    void exportProcessedSample(const juce::File& outputFile);
    
    // Playback position tracking
    float getPlaybackPosition() const { return playbackPosition.load(); }
    float getSampleLength() const { return sampleLength.load(); }
    void setPlaybackPosition(float pos) { seekPosition.store(pos); }
    
    // Parameter access
    std::atomic<float>* getGainParameter() { return &gain; }
    std::atomic<float>* getPitchParameter() { return &pitch; }
    std::atomic<float>* getSaturationParameter() { return &saturation; }
    std::atomic<float>* getSaturationTypeParameter() { return &saturationType; }
    std::atomic<float>* getProfileParameter() { return &profileType; }

    // Saturation type parameter accessors
    std::atomic<float>* getTubeDriveParameter() { return &tubeDrive; }
    std::atomic<float>* getTubeWarmthParameter() { return &tubeWarmth; }
    std::atomic<float>* getTubeBiasParameter() { return &tubeBias; }
    std::atomic<float>* getTubeOutputParameter() { return &tubeOutput; }

    std::atomic<float>* getTransistorDriveParameter() { return &transistorDrive; }
    std::atomic<float>* getTransistorBiteParameter() { return &transistorBite; }
    std::atomic<float>* getTransistorClipParameter() { return &transistorClip; }
    std::atomic<float>* getTransistorOutputParameter() { return &transistorOutput; }

    std::atomic<float>* getTapeDriveParameter() { return &tapeDrive; }
    std::atomic<float>* getTapeWowParameter() { return &tapeWow; }
    std::atomic<float>* getTapeHissParameter() { return &tapeHiss; }
    std::atomic<float>* getTapeOutputParameter() { return &tapeOutput; }

    std::atomic<float>* getDiodeDriveParameter() { return &diodeDrive; }
    std::atomic<float>* getDiodeAsymParameter() { return &diodeAsym; }
    std::atomic<float>* getDiodeClipParameter() { return &diodeClip; }
    std::atomic<float>* getDiodeOutputParameter() { return &diodeOutput; }

    std::atomic<float>* getFuzzDriveParameter() { return &fuzzDrive; }
    std::atomic<float>* getFuzzGateParameter() { return &fuzzGate; }
    std::atomic<float>* getFuzzToneParameter() { return &fuzzTone; }
    std::atomic<float>* getFuzzOutputParameter() { return &fuzzOutput; }

    std::atomic<float>* getBitDepthParameter() { return &bitDepth; }
    std::atomic<float>* getBitRateParameter() { return &bitRate; }
    std::atomic<float>* getBitMixParameter() { return &bitMix; }
    std::atomic<float>* getBitOutputParameter() { return &bitOutput; }
    
    // 6-Band Parametric EQ accessors
    std::atomic<float>* getHPFFreqParameter() { return &hpfFreq; }
    std::atomic<float>* getHPFSlopeParameter() { return &hpfSlope; }
    std::atomic<float>* getPeak1FreqParameter() { return &peak1Freq; }
    std::atomic<float>* getPeak1GainParameter() { return &peak1Gain; }
    std::atomic<float>* getPeak1QParameter() { return &peak1Q; }
    std::atomic<float>* getPeak2FreqParameter() { return &peak2Freq; }
    std::atomic<float>* getPeak2GainParameter() { return &peak2Gain; }
    std::atomic<float>* getPeak2QParameter() { return &peak2Q; }
    std::atomic<float>* getPeak3FreqParameter() { return &peak3Freq; }
    std::atomic<float>* getPeak3GainParameter() { return &peak3Gain; }
    std::atomic<float>* getPeak3QParameter() { return &peak3Q; }
    std::atomic<float>* getPeak4FreqParameter() { return &peak4Freq; }
    std::atomic<float>* getPeak4GainParameter() { return &peak4Gain; }
    std::atomic<float>* getPeak4QParameter() { return &peak4Q; }
    std::atomic<float>* getLPFFreqParameter() { return &lpfFreq; }
    std::atomic<float>* getLPFSlopeParameter() { return &lpfSlope; }
    
    // Compressor accessors
    std::atomic<float>* getCompThreshParameter() { return &compThresh; }
    std::atomic<float>* getCompRatioParameter() { return &compRatio; }
    std::atomic<float>* getCompAttackParameter() { return &compAttack; }
    std::atomic<float>* getCompReleaseParameter() { return &compRelease; }
    std::atomic<float>* getCompMakeupParameter() { return &compMakeup; }
    
    // Global Output accessor
    std::atomic<float>* getGlobalOutputParameter() { return &globalOutput; }
    
    std::atomic<bool>* getBypassParameter() { return &bypass; }    
    // Preset application
    void applyProfilePreset(int profileID);
private:
  void clearLoadedSample();
  juce::File createRecordingTempFile() const;

    //==============================================================================
    juce::Synthesiser sampler;
    juce::AudioFormatManager formatManager;
    
    // Recording state
    bool recording = false;
    juce::AudioBuffer<float> recordBuffer;
    int recordPosition = 0;
    double recordSampleRate = 44100.0;
    juce::File lastRecordingFile;
    bool clearedOnStart = false;
    
    // Parameters
    std::atomic<float> gain { 0.7f };       // 0.0 to 1.0
    std::atomic<float> pitch { 1.0f };      // 0.5 to 2.0 (semitones)
    std::atomic<float> saturation { 1.0f };  // 0.0 to 1.0 (global mix)
    std::atomic<float> saturationType { 1.0f }; // 1=Tube, 2=Transistor, 3=Tape, 4=Diode, 5=Fuzz, 6=BitCrush
    std::atomic<float> profileType { 1.0f };    // 1=Wax, 2=Vinyl, 3=Cassette, 4=Reel, 5=Neve, 6=API, 7=Speaker, 8=HiFi, 9=LoFi

    // Saturation type parameters
    std::atomic<float> tubeDrive { 4.0f };    // 0.0 to 10.0
    std::atomic<float> tubeWarmth { 0.5f };   // 0.0 to 1.0
    std::atomic<float> tubeBias { 0.0f };     // -1.0 to 1.0
    std::atomic<float> tubeOutput { 1.0f };   // 0.0 to 2.0

    std::atomic<float> transistorDrive { 4.0f };  // 0.0 to 10.0
    std::atomic<float> transistorBite { 0.5f };   // 0.0 to 1.0
    std::atomic<float> transistorClip { 0.5f };   // 0.0 to 1.0
    std::atomic<float> transistorOutput { 1.0f }; // 0.0 to 2.0

    std::atomic<float> tapeDrive { 4.0f };    // 0.0 to 10.0
    std::atomic<float> tapeWow { 0.2f };      // 0.0 to 1.0
    std::atomic<float> tapeHiss { 0.1f };     // 0.0 to 1.0
    std::atomic<float> tapeOutput { 1.0f };   // 0.0 to 2.0

    std::atomic<float> diodeDrive { 4.0f };   // 0.0 to 10.0
    std::atomic<float> diodeAsym { 0.5f };    // 0.0 to 1.0
    std::atomic<float> diodeClip { 0.5f };    // 0.0 to 1.0
    std::atomic<float> diodeOutput { 1.0f };  // 0.0 to 2.0

    std::atomic<float> fuzzDrive { 6.0f };    // 0.0 to 10.0
    std::atomic<float> fuzzGate { 0.2f };     // 0.0 to 1.0
    std::atomic<float> fuzzTone { 0.5f };     // 0.0 to 1.0
    std::atomic<float> fuzzOutput { 1.0f };   // 0.0 to 2.0

    std::atomic<float> bitDepth { 8.0f };     // 2.0 to 16.0
    std::atomic<float> bitRate { 4.0f };      // 1.0 to 16.0 (downsample factor)
    std::atomic<float> bitMix { 1.0f };       // 0.0 to 1.0
    std::atomic<float> bitOutput { 1.0f };    // 0.0 to 2.0
    
    // 6-Band Parametric EQ
    std::atomic<float> hpfFreq { 20.0f };    // 20 to 500 Hz (starts all the way left)
    std::atomic<float> hpfSlope { 1.0f };    // 1 = 12dB/oct, 2 = 24dB/oct, 8 = 96dB/oct
    std::atomic<float> peak1Freq { 200.0f }; // 20 to 500 Hz
    std::atomic<float> peak1Gain { 0.0f };   // -12 to 12 dB
    std::atomic<float> peak1Q { 1.0f };      // 0.3 to 5.0
    std::atomic<float> peak2Freq { 800.0f }; // 100 to 2000 Hz
    std::atomic<float> peak2Gain { 0.0f };   // -12 to 12 dB
    std::atomic<float> peak2Q { 1.0f };      // 0.3 to 5.0
    std::atomic<float> peak3Freq { 2000.0f };// 500 to 5000 Hz
    std::atomic<float> peak3Gain { 0.0f };   // -12 to 12 dB
    std::atomic<float> peak3Q { 1.0f };      // 0.3 to 5.0
    std::atomic<float> peak4Freq { 6000.0f };// 1000 to 10000 Hz
    std::atomic<float> peak4Gain { 0.0f };   // -12 to 12 dB  
    std::atomic<float> peak4Q { 1.0f };      // 0.3 to 5.0
    std::atomic<float> lpfFreq { 20000.0f }; // 3000 to 20000 Hz (starts all the way right)
    std::atomic<float> lpfSlope { 1.0f };    // 1 = 12dB/oct, 2 = 24dB/oct, 8 = 96dB/oct
    
    // Compressor
    std::atomic<float> compThresh { -20.0f };   // -60 to 0 dB
    std::atomic<float> compRatio { 4.0f };      // 1 to 20
    std::atomic<float> compAttack { 0.01f };    // 0.001 to 0.1 s
    std::atomic<float> compRelease { 0.1f };    // 0.01 to 1.0 s
    std::atomic<float> compMakeup { 0.0f };     // 0 to 24 dB
    
    // Global Output
    std::atomic<float> globalOutput { 0.0f };   // -24 to +6 dB, default 0 dB
    
    std::atomic<bool> bypass { false };
    
    // Playback tracking
    std::atomic<bool> shouldTriggerNote { false };
    std::atomic<bool> shouldStopNote { false };
    std::atomic<bool> loopPlayback { false };
    std::atomic<float> playbackPosition { 0.0f };
    std::atomic<float> sampleLength { 0.0f };
    std::atomic<float> seekPosition { -1.0f };
    int lastNoteTriggered = -1;
    int currentlyPlayingVoiceIndex = -1;
    int64_t samplesSinceNoteOn = 0;
    bool isNoteCurrentlyPlaying = false;
    float lastPitchValue = 1.0f;
    
    // Recording
    std::atomic<bool> recordingActive { false };
    
    // DSP state - 6-band parametric EQ
    // Multiple stages for HPF/LPF to handle slope properly
    juce::IIRFilter hpfL[8], hpfR[8];  // Up to 8 cascaded stages for 96dB/oct
    juce::IIRFilter peak1L, peak1R;
    juce::IIRFilter peak2L, peak2R;
    juce::IIRFilter peak3L, peak3R;
    juce::IIRFilter peak4L, peak4R;
    juce::IIRFilter lpfL[8], lpfR[8];  // Up to 8 cascaded stages for 96dB/oct
    
    // FET-style compressor state
    float compEnvelope = 0.0f;
    float compKneeWidth = 6.0f;  // Soft knee width in dB for FET character
    
    // Tube saturation processor
    std::unique_ptr<TubeSaturation> tubeSaturation;
    
    double currentSampleRate = 44100.0;
    double tapeWowPhase = 0.0;
    juce::Random tapeNoise;
    float fuzzToneStateL = 0.0f;
    float fuzzToneStateR = 0.0f;
    int bitcrushCounterL = 0;
    int bitcrushCounterR = 0;
    float bitcrushHoldL = 0.0f;
    float bitcrushHoldR = 0.0f;
    
    // Smoothed slope parameters to avoid clicks
    juce::SmoothedValue<float> smoothedHpfSlope;
    juce::SmoothedValue<float> smoothedLpfSlope;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaticCurrentsPluginAudioProcessor)
};
