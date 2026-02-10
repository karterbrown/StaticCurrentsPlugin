/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StaticCurrentsPluginAudioProcessor::StaticCurrentsPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // Register audio file formats (WAV, AIFF)
    formatManager.registerBasicFormats();
    
    // Add sampler voices for polyphony (8 voices)
    for (int i = 0; i < 8; ++i)
        sampler.addVoice (new juce::SamplerVoice());

    lastRecordingFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("StaticCurrentsPlugin_recording.wav");
    lastRecordingFile.deleteFile();
    clearLoadedSample();
}

StaticCurrentsPluginAudioProcessor::~StaticCurrentsPluginAudioProcessor()
{
}

//==============================================================================
const juce::String StaticCurrentsPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StaticCurrentsPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool StaticCurrentsPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool StaticCurrentsPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double StaticCurrentsPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StaticCurrentsPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int StaticCurrentsPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StaticCurrentsPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String StaticCurrentsPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void StaticCurrentsPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void StaticCurrentsPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    currentSampleRate = sampleRate;
    sampler.setCurrentPlaybackSampleRate (sampleRate);

    if (!clearedOnStart)
    {
        clearLoadedSample();
        lastRecordingFile.deleteFile();
        clearedOnStart = true;
    }
    
    // Reset all filter stages for HPF/LPF
    for (int i = 0; i < 8; ++i)
    {
        hpfL[i].reset();
        hpfR[i].reset();
        lpfL[i].reset();
        lpfR[i].reset();
    }
    
    // Reset peak filters
    peak1L.reset();
    peak1R.reset();
    peak2L.reset();
    peak2R.reset();
    
    // Initialize tube saturation processor
    if (!tubeSaturation)
        tubeSaturation = std::make_unique<TubeSaturation>();
    
    tubeSaturation->prepare(sampleRate, samplesPerBlock, 2);
    tubeSaturation->reset();
    peak3L.reset();
    peak3R.reset();
    peak4L.reset();
    peak4R.reset();
    
    // Initialize smoothed slope parameters
    smoothedHpfSlope.reset(sampleRate, 0.05); // 50ms smoothing
    smoothedLpfSlope.reset(sampleRate, 0.05);
    smoothedHpfSlope.setCurrentAndTargetValue(hpfSlope.load());
    smoothedLpfSlope.setCurrentAndTargetValue(lpfSlope.load());

    tapeWowPhase = 0.0;
    fuzzToneStateL = 0.0f;
    fuzzToneStateR = 0.0f;
    bitcrushCounterL = 0;
    bitcrushCounterR = 0;
    bitcrushHoldL = 0.0f;
    bitcrushHoldR = 0.0f;
}

void StaticCurrentsPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void StaticCurrentsPluginAudioProcessor::applyProfilePreset(int profileID)
{
    profileType.store(static_cast<float>(profileID));
    auto resetAll = [this]
    {
        gain.store(0.7f);
        pitch.store(1.0f);
        compThresh.store(-20.0f);
        compRatio.store(4.0f);
        compAttack.store(0.01f);
        compRelease.store(0.1f);
        compMakeup.store(0.0f);
        saturation.store(0.0f);
        saturationType.store(1.0f);
        tubeDrive.store(0.0f);
        tubeWarmth.store(0.0f);
        tubeBias.store(0.0f);
        tubeOutput.store(1.0f);
        transistorDrive.store(0.0f);
        transistorBite.store(0.0f);
        transistorClip.store(0.5f);
        transistorOutput.store(1.0f);
        tapeDrive.store(0.0f);
        tapeWow.store(0.0f);
        tapeHiss.store(0.0f);
        tapeOutput.store(1.0f);
        diodeDrive.store(0.0f);
        diodeAsym.store(0.5f);
        diodeClip.store(0.5f);
        diodeOutput.store(1.0f);
        fuzzDrive.store(0.0f);
        fuzzGate.store(0.0f);
        fuzzTone.store(0.5f);
        fuzzOutput.store(1.0f);
        bitDepth.store(16.0f);
        bitRate.store(1.0f);
        bitMix.store(0.0f);
        bitOutput.store(1.0f);
        hpfFreq.store(20.0f);
        hpfSlope.store(1.0f);
        peak1Freq.store(200.0f);
        peak1Gain.store(0.0f);
        peak1Q.store(1.0f);
        peak2Freq.store(1000.0f);
        peak2Gain.store(0.0f);
        peak2Q.store(1.0f);
        peak3Freq.store(3000.0f);
        peak3Gain.store(0.0f);
        peak3Q.store(1.0f);
        peak4Freq.store(6000.0f);
        peak4Gain.store(0.0f);
        peak4Q.store(1.0f);
        lpfFreq.store(20000.0f);
    };

    resetAll();
    switch (profileID)
    {
        case 0: // -init- Neutral/Default with all parameters unaffected
            break;
            
        case 1: // Wax Cylinder - vintage compressed sound with Tube saturation
            saturation.store(1.0f);  // Set saturation mix to full
            saturationType.store(1.0f); // Tube
            tubeDrive.store(7.0f);
            tubeWarmth.store(0.8f);
            tubeBias.store(0.3f);
            tubeOutput.store(1.0f);
            // EQ: Warm, compressed, rolling off highs
            hpfFreq.store(50.0f);
            peak1Freq.store(200.0f);
            peak1Gain.store(4.0f);
            peak1Q.store(0.8f);
            peak2Freq.store(800.0f);
            peak2Gain.store(2.0f);
            peak2Q.store(0.9f);
            peak3Freq.store(2500.0f);
            peak3Gain.store(-2.0f);
            peak3Q.store(1.0f);
            peak4Freq.store(6000.0f);
            peak4Gain.store(-6.0f);
            peak4Q.store(0.7f);
            lpfFreq.store(8000.0f);
            break;
            
        case 2: // Vinyl - warm tape-like with Tape saturation and hiss
            saturation.store(1.0f);
            saturationType.store(3.0f); // Tape
            tapeDrive.store(5.0f);
            tapeWow.store(0.15f);
            tapeHiss.store(0.4f);
            tapeOutput.store(1.0f);
            // EQ: Gentle bass boost, high-end presence with surface noise
            hpfFreq.store(40.0f);
            peak1Freq.store(150.0f);
            peak1Gain.store(3.0f);
            peak1Q.store(0.7f);
            peak2Freq.store(1000.0f);
            peak2Gain.store(1.0f);
            peak2Q.store(1.0f);
            peak3Freq.store(3000.0f);
            peak3Gain.store(2.0f);
            peak3Q.store(1.2f);
            peak4Freq.store(7000.0f);
            peak4Gain.store(-3.0f);
            peak4Q.store(0.8f);
            lpfFreq.store(12000.0f);
            break;
            
        case 3: // Cassette - tape with heavy wow and hiss
            saturation.store(1.0f);
            saturationType.store(3.0f); // Tape
            tapeDrive.store(6.0f);
            tapeWow.store(0.7f);
            tapeHiss.store(0.6f);
            tapeOutput.store(0.95f);
            // EQ: Compressed mids, reduced highs due to tape saturation
            hpfFreq.store(60.0f);
            peak1Freq.store(180.0f);
            peak1Gain.store(2.5f);
            peak1Q.store(0.8f);
            peak2Freq.store(900.0f);
            peak2Gain.store(-1.0f);
            peak2Q.store(1.1f);
            peak3Freq.store(2800.0f);
            peak3Gain.store(1.5f);
            peak3Q.store(0.9f);
            peak4Freq.store(5000.0f);
            peak4Gain.store(-5.0f);
            peak4Q.store(0.7f);
            lpfFreq.store(7000.0f);
            break;
            
        case 4: // Reel to Reel - clean professional tape
            saturation.store(1.0f);
            saturationType.store(3.0f); // Tape
            tapeDrive.store(3.0f);
            tapeWow.store(0.05f);
            tapeHiss.store(0.05f);
            tapeOutput.store(1.0f);
            // EQ: Flat response, minimal coloration
            hpfFreq.store(20.0f);
            peak1Freq.store(200.0f);
            peak1Gain.store(0.0f);
            peak1Q.store(1.0f);
            peak2Freq.store(1000.0f);
            peak2Gain.store(0.0f);
            peak2Q.store(1.0f);
            peak3Freq.store(3000.0f);
            peak3Gain.store(0.0f);
            peak3Q.store(1.0f);
            peak4Freq.store(6000.0f);
            peak4Gain.store(0.0f);
            peak4Q.store(1.0f);
            lpfFreq.store(20000.0f);
            break;
            
        case 5: // Neve - smooth warm console with Tube saturation
            saturation.store(1.0f);
            saturationType.store(1.0f); // Tube
            tubeDrive.store(4.0f);
            tubeWarmth.store(0.7f);
            tubeBias.store(0.1f);
            tubeOutput.store(1.05f);
            // EQ: Smooth warm with low-end presence and presence peak
            hpfFreq.store(30.0f);
            peak1Freq.store(200.0f);
            peak1Gain.store(2.5f);
            peak1Q.store(0.75f);
            peak2Freq.store(600.0f);
            peak2Gain.store(1.0f);
            peak2Q.store(0.9f);
            peak3Freq.store(3500.0f);
            peak3Gain.store(3.0f);
            peak3Q.store(1.1f);
            peak4Freq.store(8000.0f);
            peak4Gain.store(-2.0f);
            peak4Q.store(0.8f);
            lpfFreq.store(18000.0f);
            break;
            
        case 6: // API - bright punchy console with Transistor
            saturation.store(1.0f);
            saturationType.store(2.0f); // Transistor
            transistorDrive.store(5.0f);
            transistorBite.store(0.7f);
            transistorClip.store(0.3f);
            transistorOutput.store(1.0f);
            // EQ: Bright presence with aggressive upper mids
            hpfFreq.store(35.0f);
            peak1Freq.store(150.0f);
            peak1Gain.store(2.0f);
            peak1Q.store(0.8f);
            peak2Freq.store(750.0f);
            peak2Gain.store(3.0f);
            peak2Q.store(1.0f);
            peak3Freq.store(3000.0f);
            peak3Gain.store(4.0f);
            peak3Q.store(1.2f);
            peak4Freq.store(6000.0f);
            peak4Gain.store(2.0f);
            peak4Q.store(0.9f);
            lpfFreq.store(20000.0f);
            break;
            
        case 7: // Blown Speaker - extreme degraded sound with Fuzz
            saturation.store(1.0f);
            saturationType.store(5.0f); // Fuzz
            fuzzDrive.store(9.0f);
            fuzzGate.store(0.4f);
            fuzzTone.store(0.2f);
            fuzzOutput.store(0.8f);
            // EQ: Heavily filtered, crushed mids, rolled off everything
            hpfFreq.store(80.0f);
            peak1Freq.store(200.0f);
            peak1Gain.store(-2.0f);
            peak1Q.store(0.7f);
            peak2Freq.store(800.0f);
            peak2Gain.store(-4.0f);
            peak2Q.store(0.8f);
            peak3Freq.store(2000.0f);
            peak3Gain.store(-3.0f);
            peak3Q.store(0.9f);
            peak4Freq.store(5000.0f);
            peak4Gain.store(-6.0f);
            peak4Q.store(0.7f);
            lpfFreq.store(4000.0f);
            break;
            
        case 8: // HiFi - clean minimal saturation with Diode
            saturation.store(1.0f);
            saturationType.store(4.0f); // Diode
            diodeDrive.store(1.0f);
            diodeAsym.store(0.5f);
            diodeClip.store(0.2f);
            diodeOutput.store(1.05f);
            // EQ: Bright, clear, extended treble
            hpfFreq.store(20.0f);
            peak1Freq.store(200.0f);
            peak1Gain.store(1.0f);
            peak1Q.store(0.9f);
            peak2Freq.store(1000.0f);
            peak2Gain.store(0.5f);
            peak2Q.store(1.0f);
            peak3Freq.store(4000.0f);
            peak3Gain.store(2.0f);
            peak3Q.store(1.0f);
            peak4Freq.store(8000.0f);
            peak4Gain.store(3.0f);
            peak4Q.store(1.1f);
            lpfFreq.store(20000.0f);
            break;
            
        case 9: // LoFi - heavily degraded with Bitcrush
            saturation.store(1.0f);
            saturationType.store(6.0f); // Bitcrush
            bitDepth.store(6.0f);
            bitRate.store(8.0f);
            bitMix.store(0.9f);
            bitOutput.store(0.85f);
            // EQ: Extreme filtering, telephone/radio effect
            hpfFreq.store(100.0f);
            peak1Freq.store(150.0f);
            peak1Gain.store(-3.0f);
            peak1Q.store(0.7f);
            peak2Freq.store(600.0f);
            peak2Gain.store(5.0f);
            peak2Q.store(0.8f);
            peak3Freq.store(2000.0f);
            peak3Gain.store(3.0f);
            peak3Q.store(0.9f);
            peak4Freq.store(5000.0f);
            peak4Gain.store(-5.0f);
            peak4Q.store(0.8f);
            lpfFreq.store(3500.0f);
            break;
            
        default:
            break;
    }
    
    profileType.store(static_cast<float>(profileID));
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StaticCurrentsPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Support mono or stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Allow stereo input for recording (even though this is a synth)
    // Input can be disabled (empty), mono, or stereo
    auto inputChannels = layouts.getMainInputChannelSet();
    if (inputChannels != juce::AudioChannelSet::disabled()
     && inputChannels != juce::AudioChannelSet::mono()
     && inputChannels != juce::AudioChannelSet::stereo())
        return false;

    return true;
  #endif
}
#endif

void StaticCurrentsPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Trigger sample playback if requested
    if (shouldTriggerNote.exchange(false) && sampler.getNumSounds() > 0)
    {
        DBG("TRIGGERING NEW NOTE - Play button pressed");
        
        // Calculate MIDI note based on pitch parameter
        // pitch: 0.5 = -12 semitones, 1.0 = 0 semitones, 2.0 = +12 semitones
        float pitchValue = pitch.load();
        float semitones = 12.0f * std::log2(pitchValue);
        int midiNote = 60 + static_cast<int>(std::round(semitones));
        midiNote = juce::jlimit(0, 127, midiNote);
        
        lastNoteTriggered = midiNote;
        lastPitchValue = pitchValue;
        samplesSinceNoteOn = 0;
        isNoteCurrentlyPlaying = true;
        midiMessages.addEvent(juce::MidiMessage::noteOn(1, midiNote, (juce::uint8)100), 0);
    }
    
    // Stop sample playback if requested
    if (shouldStopNote.exchange(false))
    {
        DBG("STOP requested - stopping all playback");
        sampler.allNotesOff(1, true);
        if (lastNoteTriggered >= 0)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastNoteTriggered), 0);
            lastNoteTriggered = -1;
        }
        isNoteCurrentlyPlaying = false;
        samplesSinceNoteOn = 0;
        
        // Prevent any pending trigger
        shouldTriggerNote.store(false);
    }

    // Recording incoming audio - copy input before clearing
    if (recording && totalNumInputChannels > 0)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, totalNumInputChannels);
        
        // Debug: Check if we're actually receiving input
        static int debugCounter = 0;
        if (++debugCounter % 100 == 0) // Log every 100 blocks
        {
            float maxLevel = 0.0f;
            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                auto* data = buffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    maxLevel = juce::jmax(maxLevel, std::abs(data[i]));
            }
            DBG("Recording - Input Channels: " + juce::String(totalNumInputChannels) +
                ", Samples: " + juce::String(numSamples) +
                ", Max Level: " + juce::String(maxLevel, 4) +
                ", Position: " + juce::String(recordPosition));
        }
        
        // Ensure we have space in the record buffer
        if (recordPosition + numSamples < recordBuffer.getNumSamples())
        {
            // Copy from input buffer to record buffer
            for (int ch = 0; ch < numChannels; ++ch)
            {
                recordBuffer.copyFrom (ch, recordPosition, buffer.getReadPointer(ch), numSamples);
            }
            recordPosition += numSamples;
        }
        else
        {
            // Buffer full, stop recording
            stopRecording();
        }
    }

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Only clear input channels if NOT recording (to prevent feedback during playback)
    if (!recording)
    {
        for (auto i = 0; i < totalNumInputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());
    }

    // Seek functionality disabled to prevent unintended looping
    seekPosition.store(-1.0f);

    // CRITICAL: Stop playback BEFORE rendering if we're near the end to prevent looping
    if (lastNoteTriggered >= 0 && isNoteCurrentlyPlaying)
    {
        float baseSampleLength = sampleLength.load();
        
        if (baseSampleLength > 0.0f && currentSampleRate > 0.0)
        {
            // Account for pitch: pitch changes the playback rate
            float currentPitchValue = lastPitchValue;
            float actualDuration = baseSampleLength / currentPitchValue;
            
            // Check current position BEFORE rendering this buffer
            float currentPos = static_cast<float>(samplesSinceNoteOn) / static_cast<float>(currentSampleRate);
            
            // Stop WELL BEFORE the end (200ms buffer) to absolutely prevent any looping
            if (currentPos >= (actualDuration - 0.2f))
            {
                DBG("Stopping playback near end. Position: " + juce::String(currentPos, 3) + 
                    ", Duration: " + juce::String(actualDuration, 3));
                
                // Force stop immediately and prevent any retriggering
                isNoteCurrentlyPlaying = false;
                sampler.allNotesOff(1, true);
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastNoteTriggered), 0);
                lastNoteTriggered = -1;
                samplesSinceNoteOn = 0;
                playbackPosition.store(0.0f);
                
                // CRITICAL: Clear any pending trigger to prevent loop restart
                shouldTriggerNote.store(false);
            }
        }
    }
    
    // Render sampler output
    sampler.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    
    // Update playback position after rendering
    if (lastNoteTriggered >= 0 && isNoteCurrentlyPlaying)
    {
        samplesSinceNoteOn += buffer.getNumSamples();
        float baseSampleLength = sampleLength.load();
        
        if (baseSampleLength > 0.0f && currentSampleRate > 0.0)
        {
            float currentPitchValue = lastPitchValue;
            float actualDuration = baseSampleLength / currentPitchValue;
            float currentPos = static_cast<float>(samplesSinceNoteOn) / static_cast<float>(currentSampleRate);
            playbackPosition.store(currentPos);
        }
    }
    else if (lastNoteTriggered < 0)
    {
        playbackPosition.store(0.0f);
        samplesSinceNoteOn = 0;
    }
    
    // Update sample length tracking
    if (sampler.getNumSounds() > 0)
    {
        auto* sound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(0).get());
        if (sound != nullptr)
        {
            sampleLength.store(static_cast<float>(sound->getAudioData()->getNumSamples()) / static_cast<float>(currentSampleRate));
        }
    }
    
    // Apply effects chain if not bypassed
    if (!bypass.load())
    {
        const int numSamples = buffer.getNumSamples();
        
        // 1. Gain (applied first, before any processing)
        float currentGain = gain.load();
        buffer.applyGain (currentGain);
        
        // 2. 6-Band Parametric EQ - Update filter coefficients
        // Smooth slope parameter changes to avoid clicks
        smoothedHpfSlope.setTargetValue(hpfSlope.load());
        smoothedLpfSlope.setTargetValue(lpfSlope.load());
        
        // Skip smoothing for this buffer and just get the current target
        // This updates the internal state without processing sample-by-sample
        smoothedHpfSlope.skip(numSamples);
        smoothedLpfSlope.skip(numSamples);
        
        // HPF (High-pass filter) - Use Butterworth response for smooth curves
        auto hpf_freq = hpfFreq.load();
        float hpf_slope_value = smoothedHpfSlope.getCurrentValue();
        int hpf_stages = (hpf_slope_value > 0.0f) ? juce::jlimit(1, 8, static_cast<int>(std::round(hpf_slope_value))) : 0;
        
        if (hpf_stages > 0)
        {
            auto hpfCoeffs = juce::IIRCoefficients::makeHighPass(currentSampleRate, hpf_freq, 0.707);
            for (int i = 0; i < 8; ++i)
            {
                hpfL[i].setCoefficients(hpfCoeffs);
                hpfR[i].setCoefficients(hpfCoeffs);
            }
        }
        
        // Peak Band 1
        auto p1_freq = peak1Freq.load();
        auto p1_gain = peak1Gain.load();
        auto p1_q = peak1Q.load();
        auto peak1Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p1_freq, p1_q, juce::Decibels::decibelsToGain(p1_gain * 1.5f));
        peak1L.setCoefficients(peak1Coeffs);
        peak1R.setCoefficients(peak1Coeffs);
        
        // Peak Band 2
        auto p2_freq = peak2Freq.load();
        auto p2_gain = peak2Gain.load();
        auto p2_q = peak2Q.load();
        auto peak2Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p2_freq, p2_q, juce::Decibels::decibelsToGain(p2_gain * 1.5f));
        peak2L.setCoefficients(peak2Coeffs);
        peak2R.setCoefficients(peak2Coeffs);
        
        // Peak Band 3
        auto p3_freq = peak3Freq.load();
        auto p3_gain = peak3Gain.load();
        auto p3_q = peak3Q.load();
        auto peak3Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p3_freq, p3_q, juce::Decibels::decibelsToGain(p3_gain * 1.5f));
        peak3L.setCoefficients(peak3Coeffs);
        peak3R.setCoefficients(peak3Coeffs);
        
        // Peak Band 4
        auto p4_freq = peak4Freq.load();
        auto p4_gain = peak4Gain.load();
        auto p4_q = peak4Q.load();
        auto peak4Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p4_freq, p4_q, juce::Decibels::decibelsToGain(p4_gain * 1.5f));
        peak4L.setCoefficients(peak4Coeffs);
        peak4R.setCoefficients(peak4Coeffs);
        
        // LPF (Low-pass filter) - Use Butterworth response for smooth curves
        auto lpf_freq = lpfFreq.load();
        float lpf_slope_value = smoothedLpfSlope.getCurrentValue();
        int lpf_stages = (lpf_slope_value > 0.0f) ? juce::jlimit(1, 8, static_cast<int>(std::round(lpf_slope_value))) : 0;
        
        if (lpf_stages > 0)
        {
            auto lpfCoeffs = juce::IIRCoefficients::makeLowPass(currentSampleRate, lpf_freq, 0.707);
            for (int i = 0; i < 8; ++i)
            {
                lpfL[i].setCoefficients(lpfCoeffs);
                lpfR[i].setCoefficients(lpfCoeffs);
            }
        }
        
        // Apply all EQ bands in series
        if (buffer.getNumChannels() > 0)
        {
            // HPF - Apply cascaded stages for Butterworth response (only if slope > 0)
            if (hpf_stages > 0)
            {
                for (int stage = 0; stage < hpf_stages; ++stage)
                    hpfL[stage].processSamples(buffer.getWritePointer(0), numSamples);
            }
            
            peak1L.processSamples(buffer.getWritePointer(0), numSamples);
            peak2L.processSamples(buffer.getWritePointer(0), numSamples);
            peak3L.processSamples(buffer.getWritePointer(0), numSamples);
            peak4L.processSamples(buffer.getWritePointer(0), numSamples);
            
            // LPF - Apply cascaded stages for Butterworth response (only if slope > 0)
            if (lpf_stages > 0)
            {
                for (int stage = 0; stage < lpf_stages; ++stage)
                    lpfL[stage].processSamples(buffer.getWritePointer(0), numSamples);
            }
        }
        if (buffer.getNumChannels() > 1)
        {
            // HPF - Apply cascaded stages for Butterworth response (only if slope > 0)
            if (hpf_stages > 0)
            {
                for (int stage = 0; stage < hpf_stages; ++stage)
                    hpfR[stage].processSamples(buffer.getWritePointer(1), numSamples);
            }
            
            peak1R.processSamples(buffer.getWritePointer(1), numSamples);
            peak2R.processSamples(buffer.getWritePointer(1), numSamples);
            peak3R.processSamples(buffer.getWritePointer(1), numSamples);
            peak4R.processSamples(buffer.getWritePointer(1), numSamples);
            
            // LPF - Apply cascaded stages for Butterworth response (only if slope > 0)
            if (lpf_stages > 0)
            {
                for (int stage = 0; stage < lpf_stages; ++stage)
                    lpfR[stage].processSamples(buffer.getWritePointer(1), numSamples);
            }
        }
        
        // 3. FET-Style Compression (1176-inspired)
        float threshold = compThresh.load();
        float ratio = compRatio.load();
        float attackTime = compAttack.load();
        float releaseTime = compRelease.load();
        float makeup = compMakeup.load();
        
        // FET compressors have faster time constants
        float attackCoeff = 1.0f - std::exp(-1.0f / (attackTime * static_cast<float>(currentSampleRate) * 0.5f));
        float releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * static_cast<float>(currentSampleRate)));
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Peak detection
            float peak = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));
            
            float peakDb = juce::Decibels::gainToDecibels (peak + 0.0001f);
            float gainReduction = 0.0f;
            
            // Soft-knee compression (FET characteristic)
            if (peakDb > threshold - compKneeWidth / 2.0f)
            {
                if (peakDb < threshold + compKneeWidth / 2.0f)
                {
                    // In the knee region - smooth transition
                    float kneeInput = peakDb - threshold + compKneeWidth / 2.0f;
                    float kneeSquared = kneeInput * kneeInput;
                    gainReduction = kneeSquared / (2.0f * compKneeWidth) * (1.0f - 1.0f / ratio);
                }
                else
                {
                    // Above knee - full compression with slight FET saturation
                    float excess = peakDb - threshold;
                    gainReduction = excess * (1.0f - 1.0f / ratio);
                    
                    // Add FET-style harmonic saturation at high compression (more pronounced)
                    if (gainReduction > 10.0f)
                    {
                        float satAmount = (gainReduction - 10.0f) * 0.05f;
                        gainReduction += satAmount * satAmount;
                    }
                }
            }
            
            // Envelope follower with FET-style timing
            float coeff = (gainReduction > compEnvelope) ? attackCoeff : releaseCoeff;
            compEnvelope += (gainReduction - compEnvelope) * coeff;
            
            // Denormal protection for envelope
            if (std::abs(compEnvelope) < 1e-15f)
                compEnvelope = 0.0f;
            
            // Apply compression with makeup gain and FET-style slight odd harmonics
            float compGain = juce::Decibels::decibelsToGain (-compEnvelope + makeup);
            
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                float sample = buffer.getSample (ch, i) * compGain;
                
                // FET-style coloration (odd harmonics) when compressing - more pronounced
                if (compEnvelope > 3.0f)
                {
                    float colorAmount = juce::jmin(compEnvelope * 0.02f, 0.15f);
                    sample = sample + colorAmount * std::tanh(sample * 3.0f) * 0.2f;
                }
                
                // NaN/Inf protection for compressor output
                if (std::isnan(sample) || std::isinf(sample))
                    sample = buffer.getSample(ch, i);  // Fall back to unprocessed
                
                buffer.setSample (ch, i, sample);
            }
        }
        
        // 4. Saturation (Post-Compression)
        float satMix = juce::jlimit (0.0f, 1.0f, saturation.load());
        int satType = static_cast<int>(saturationType.load());
        int profile = static_cast<int>(profileType.load());
        
        // Mode 1: Tube Saturation (dedicated processor with oversampling)
        if (satType == 1 && tubeSaturation != nullptr)
        {
            // Update parameters from atomics
            float drive = tubeDrive.load();
            float warmth = tubeWarmth.load();
            float bias = tubeBias.load();
            float output = tubeOutput.load();
            
            // Map warmth 0-1 to dB range -12 to +6
            float warmthDb = (warmth * 18.0f) - 12.0f;
            
            // Bias is already in the correct range -1 to +1
            float biasAmount = bias;
            
            // Map output 0-2 to dB range -12 to +12
            float outputDb = (output - 1.0f) * 12.0f;
            
            tubeSaturation->setDrive(drive);
            tubeSaturation->setWarmth(warmthDb);
            tubeSaturation->setBias(biasAmount);
            tubeSaturation->setOutputGain(outputDb);
            
            // Process buffer with oversampled tube saturation
            tubeSaturation->process(buffer);
        }
        else
        {
            // Legacy saturation modes (Transistor, Tape, Diode, Fuzz, Bitcrusher)
            // Read saturation parameters directly - NO profile-based modifiers
            // This ensures manual parameter adjustments work correctly on ANY sample
            
            if (satMix > 0.0f)
            {
                // Read ALL saturation parameters directly from atomics without scaling
                float tubeDriveVal = tubeDrive.load();
                float tubeWarmthVal = tubeWarmth.load();
                float tubeBiasVal = tubeBias.load();
                float tubeOutVal = tubeOutput.load();
    
                float transistorDriveVal = transistorDrive.load();
                float transistorBiteVal = transistorBite.load();
                float transistorClipVal = transistorClip.load();
                float transistorOutVal = transistorOutput.load();
    
                float tapeDriveVal = tapeDrive.load();
                float tapeWowVal = tapeWow.load();
                float tapeHissVal = tapeHiss.load();
                float tapeOutVal = tapeOutput.load();
    
                float diodeDriveVal = diodeDrive.load();
                float diodeAsymVal = diodeAsym.load();
                float diodeClipVal = diodeClip.load();
                float diodeOutVal = diodeOutput.load();
    
                float fuzzDriveVal = fuzzDrive.load();
                float fuzzGateVal = fuzzGate.load();
                float fuzzToneVal = fuzzTone.load();
                float fuzzOutVal = fuzzOutput.load();
    
                float bitDepthVal = bitDepth.load();
                float bitRateVal = bitRate.load();
                float bitMixVal = bitMix.load();
                float bitOutVal = bitOutput.load();

            const float wowRate = 0.2f + (tapeWowVal * 2.0f);
            const float wowInc = static_cast<float>((juce::MathConstants<double>::twoPi * wowRate) / currentSampleRate);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                int& crushCounter = (ch == 0) ? bitcrushCounterL : bitcrushCounterR;
                float& crushHold = (ch == 0) ? bitcrushHoldL : bitcrushHoldR;
                float& fuzzState = (ch == 0) ? fuzzToneStateL : fuzzToneStateR;

                for (int i = 0; i < numSamples; ++i)
                {
                    float dry = data[i];
                    float processed = dry;

                    float tubeWeight = (tubeDriveVal / 10.0f) * 0.65f
                                     + tubeWarmthVal * 0.18f
                                     + std::abs (tubeBiasVal) * 0.07f
                                     + std::abs (tubeOutVal - 1.0f) * 0.10f;
                    float transistorWeight = (transistorDriveVal / 10.0f) * 0.55f
                                           + transistorBiteVal * 0.22f
                                           + transistorClipVal * 0.13f
                                           + std::abs (transistorOutVal - 1.0f) * 0.10f;
                    float tapeWeight = (tapeDriveVal / 10.0f) * 0.55f
                                     + tapeWowVal * 0.18f
                                     + tapeHissVal * 0.17f
                                     + std::abs (tapeOutVal - 1.0f) * 0.10f;
                    float diodeWeight = (diodeDriveVal / 10.0f) * 0.55f
                                      + diodeAsymVal * 0.18f
                                      + diodeClipVal * 0.17f
                                      + std::abs (diodeOutVal - 1.0f) * 0.10f;
                    float fuzzWeight = (fuzzDriveVal / 10.0f) * 0.55f
                                     + fuzzGateVal * 0.15f
                                     + (1.0f - fuzzToneVal) * 0.20f
                                     + std::abs (fuzzOutVal - 1.0f) * 0.10f;
                    float depthWeight = (16.0f - bitDepthVal) / 14.0f;
                    float rateWeight = (bitRateVal - 1.0f) / 15.0f;
                    float bitWeight = bitMixVal * 0.55f
                                    + depthWeight * 0.2f
                                    + rateWeight * 0.15f
                                    + std::abs (bitOutVal - 1.0f) * 0.10f;

                    tubeWeight = juce::jlimit (0.0f, 1.0f, tubeWeight);
                    transistorWeight = juce::jlimit (0.0f, 1.0f, transistorWeight);
                    tapeWeight = juce::jlimit (0.0f, 1.0f, tapeWeight);
                    diodeWeight = juce::jlimit (0.0f, 1.0f, diodeWeight);
                    fuzzWeight = juce::jlimit (0.0f, 1.0f, fuzzWeight);
                    bitWeight = juce::jlimit (0.0f, 1.0f, bitWeight);

                    const float focusBoost = 1.1f;
                    if (satType == 1) tubeWeight *= focusBoost;
                    if (satType == 2) transistorWeight *= focusBoost;
                    if (satType == 3) tapeWeight *= focusBoost;
                    if (satType == 4) diodeWeight *= focusBoost;
                    if (satType == 5) fuzzWeight *= focusBoost;
                    if (satType == 6) bitWeight *= focusBoost;

                    // Input attenuation for hot saturation modes (like real analog gear)
                    float preAtten = 0.7f;
                    float dryScaled = dry * preAtten;

                    float tubeDrive = 1.0f + tubeDriveVal * 0.6f;  // Increased for more extreme effect (max 7x)
                    float tubeBias = tubeBiasVal * 0.5f;  // Doubled bias shift for more pronounced effect
                    float tubeWarm = (0.6f + tubeWarmthVal * 2.0f);  // Increased warmth effect
                    float tubeDriven = (dryScaled + tubeBias) * tubeDrive;
                    float tubeEven = std::abs (tubeDriven) * tubeDriven * (0.25f * tubeWarmthVal);  // More even harmonics
                    float tubeSat = std::tanh ((tubeDriven + tubeEven) * tubeWarm);
                    float tubeComp = 1.0f / (1.0f + std::abs (tubeSat) * 0.6f);
                    float tubeOut = tubeSat * tubeComp * tubeOutVal / preAtten;  // Compensate attenuation

                    float transDrive = 1.0f + transistorDriveVal * 0.6f;  // Increased for more extreme effect (max 7x)
                    float transBite = juce::jlimit (0.0f, 1.0f, transistorBiteVal);
                    float transClip = 0.9f - (transistorClipVal * 0.7f);
                    float transDriven = dryScaled * transDrive;
                    float transClipped = juce::jlimit (-transClip, transClip, transDriven);
                    float transSoft = std::tanh (transClipped * (1.0f + transBite * 4.0f));  // More aggressive bite effect
                    float transHard = transClipped / transClip;
                    float transSat = juce::jlimit (-1.0f, 1.0f, transSoft * (1.0f - transBite) + transHard * transBite);
                    
                    // Add crossover distortion (transistor characteristic)
                    float crossover = transSat * 0.05f * (1.0f - std::abs(transSat));  // More pronounced crossover
                    transSat += crossover * transBite;
                    
                    float transOut = transSat * transistorOutVal / preAtten;  // Compensate attenuation

                    tapeWowPhase += wowInc;
                    if (tapeWowPhase > juce::MathConstants<double>::twoPi)
                        tapeWowPhase -= juce::MathConstants<double>::twoPi;

                    float wowMod = 1.0f + std::sin (static_cast<float>(tapeWowPhase)) * tapeWowVal * 0.05f;  // More wow/flutter
                    float tapeDrive = 1.0f + tapeDriveVal * 1.0f * wowMod;  // Increased drive (max 11x)
                    float tapeDriven = dryScaled * tapeDrive;
                    float tapeComp = tapeDriven / (1.0f + std::abs (tapeDriven) * 0.7f);
                    float tapeSat = std::tanh (tapeComp * 1.12f);
                    float tapeLoss = 1.0f - tapeHissVal * 0.6f;  // More pronounced hiss/loss
                    float tapeOut = tapeSat * tapeLoss * tapeOutVal / preAtten;  // Compensate attenuation

                    float diodeDrive = 1.0f + diodeDriveVal * 0.7f;  // Increased for more extreme effect (max 8x) (max 4.5x instead of 13x)
                    float diodeAsym = juce::jlimit (0.0f, 1.0f, diodeAsymVal);
                    float diodeClip = 0.95f - (diodeClipVal * 0.75f);
                    float diodeDriven = dryScaled * diodeDrive;
                    float diodeClipped = juce::jlimit (-diodeClip, diodeClip, diodeDriven);
                    float diodeRect = (1.0f - diodeAsym) * diodeClipped + diodeAsym * std::abs (diodeClipped);
                    
                    // Forward voltage drop simulation (0.6V diode characteristic)
                    float fwdDrop = 0.6f / 10.0f;  // Normalized
                    if (diodeRect > fwdDrop)
                        diodeRect = diodeRect - fwdDrop;
                    else if (diodeRect < -fwdDrop)
                        diodeRect = diodeRect + fwdDrop;
                    else
                        diodeRect = 0.0f;
                    
                    float diodeSat = std::tanh (diodeRect * (1.2f + diodeClipVal * 2.0f));  // More harmonic distortion
                    float diodeOut = diodeSat * diodeOutVal / preAtten;  // Compensate attenuation

                    float fuzzDrive = 1.0f + fuzzDriveVal * 0.7f;  // Increased for more extreme effect (max 8x) (max 5x instead of 17x) - CRITICAL FIX
                    float fuzzGate = fuzzGateVal * 0.12f;  // More aggressive gating
                    float fuzzTone = juce::jlimit (0.0f, 1.0f, fuzzToneVal);
                    float fuzzDriven = dryScaled * fuzzDrive;
                    float fuzzed = juce::jlimit (-1.0f, 1.0f, fuzzDriven);
                    if (std::abs (fuzzed) < fuzzGate)
                        fuzzed *= std::abs (fuzzed) / juce::jmax (0.001f, fuzzGate);
                    
                    // Add octave-up effect (fuzz characteristic - frequency doubling)
                    float octaveUp = std::abs(fuzzed) * fuzzed * 0.25f;  // More octave-up harmonics
                    fuzzed = fuzzed * 0.75f + octaveUp;  // Adjusted mix for stronger effect
                    
                    float fuzzAlpha = 0.08f + (1.0f - fuzzTone) * 0.6f;  // More extreme tone shaping
                    fuzzState += fuzzAlpha * (fuzzed - fuzzState);
                    float fuzzOut = fuzzState * fuzzOutVal / preAtten;  // Compensate attenuation

                    int bits = juce::jlimit (2, 16, static_cast<int>(std::round (bitDepthVal)));
                    int rate = juce::jlimit (1, 16, static_cast<int>(std::round (bitRateVal)));
                    float step = 2.0f / static_cast<float> (1 << bits);

                    float crushSample = dryScaled;
                    if (rate > 1)
                    {
                        if (crushCounter <= 0)
                        {
                            crushHold = crushSample;
                            crushCounter = rate - 1;
                        }
                        else
                        {
                            --crushCounter;
                            crushSample = crushHold;
                        }
                    }

                    float quant = std::floor (crushSample / step) * step;
                    float bitWet = juce::jlimit (0.0f, 1.0f, bitMixVal);
                    float bitOut = (dryScaled * (1.0f - bitWet) + quant * bitWet) * bitOutVal / preAtten;  // Compensate attenuation

                    float weightSum = tubeWeight + transistorWeight + tapeWeight + diodeWeight + fuzzWeight + bitWeight;
                    if (weightSum < 0.0001f)
                    {
                        processed = dry;
                    }
                    else
                    {
                        processed = (tubeOut * tubeWeight
                                     + transOut * transistorWeight
                                     + tapeOut * tapeWeight
                                     + diodeOut * diodeWeight
                                     + fuzzOut * fuzzWeight
                                     + bitOut * bitWeight) / weightSum;
                    }

                    data[i] = dry * (1.0f - satMix) + processed * satMix;
                }
            }
        }
        } // end else (non-Tube saturation modes)
        
        // 5. Final Global Output Trim + Safety Limiter (applied to all modes)
        float globalOutDb = globalOutput.load();
        float globalGain = juce::Decibels::decibelsToGain(globalOutDb);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = data[i] * globalGain;
                
                // NaN/Inf protection
                if (std::isnan(sample) || std::isinf(sample))
                    sample = 0.0f;
                
                // Denormal protection
                if (std::abs(sample) < 1e-15f)
                    sample = 0.0f;
                
                // Soft clipper/limiter (prevents runaway peaks)
                if (sample > 1.0f)
                    sample = 1.0f + std::tanh((sample - 1.0f) * 0.5f) * 0.1f;
                else if (sample < -1.0f)
                    sample = -1.0f + std::tanh((sample + 1.0f) * 0.5f) * 0.1f;
                
                data[i] = sample;
            }
        }
    }
    else
    {
        // When bypassed, still apply gain and global output
        float currentGain = gain.load();
        float globalOutDb = globalOutput.load();
        float globalGain = juce::Decibels::decibelsToGain(globalOutDb);
        buffer.applyGain (currentGain * globalGain);
    }
}

//==============================================================================
bool StaticCurrentsPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* StaticCurrentsPluginAudioProcessor::createEditor()
{
    return new StaticCurrentsPluginAudioProcessorEditor (*this);
}

//==============================================================================
void StaticCurrentsPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void StaticCurrentsPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void StaticCurrentsPluginAudioProcessor::startRecording()
{
    if (!recording)
    {
        shouldStopNote.store(true);
        shouldTriggerNote.store(false);
        loopPlayback.store(false);
        clearLoadedSample();
        lastRecordingFile.deleteFile();
        recording = true;
        recordingActive.store(true);
        recordPosition = 0;
        recordSampleRate = getSampleRate();
        
        // Allocate 30 seconds of stereo recording buffer
        int maxSamples = static_cast<int> (recordSampleRate * 30.0);
        recordBuffer.setSize (2, maxSamples, false, true, true);
        recordBuffer.clear();
    }
}

void StaticCurrentsPluginAudioProcessor::clearLoadedSample()
{
    sampler.allNotesOff(1, true);
    sampler.clearSounds();
    sampleLength.store(0.0f);
    playbackPosition.store(0.0f);
    seekPosition.store(-1.0f);
    lastNoteTriggered = -1;
    currentlyPlayingVoiceIndex = -1;
    samplesSinceNoteOn = 0;
    isNoteCurrentlyPlaying = false;
}

void StaticCurrentsPluginAudioProcessor::stopRecording()
{
    if (!recording)
        return;

    recording = false;
    recordingActive.store(false);

    DBG("stopRecording called. recordPosition: " + juce::String(recordPosition));

    if (recordPosition > 0)
    {
        if (recordSampleRate <= 0.0)
        {
            DBG("ERROR: recordSampleRate is invalid: " + juce::String(recordSampleRate));
            clearLoadedSample();
            recordPosition = 0;
            return;
        }

        // Trim the buffer to actual recorded length
        juce::AudioBuffer<float> trimmedBuffer (recordBuffer.getNumChannels(), recordPosition);
        for (int ch = 0; ch < recordBuffer.getNumChannels(); ++ch)
            trimmedBuffer.copyFrom (ch, 0, recordBuffer, ch, 0, recordPosition);
        
        // Calculate peak level to verify we have audio data
        float peakLevel = 0.0f;
        for (int ch = 0; ch < trimmedBuffer.getNumChannels(); ++ch)
        {
            auto* data = trimmedBuffer.getReadPointer(ch);
            for (int i = 0; i < trimmedBuffer.getNumSamples(); ++i)
                peakLevel = juce::jmax(peakLevel, std::abs(data[i]));
        }
        DBG("Trimmed buffer - Channels: " + juce::String(trimmedBuffer.getNumChannels()) +
            ", Samples: " + juce::String(trimmedBuffer.getNumSamples()) +
            ", Peak Level: " + juce::String(peakLevel, 4));
        
        // Create a temporary file to save the recording
        lastRecordingFile = createRecordingTempFile();
        DBG("Saving to: " + lastRecordingFile.getFullPathName());
        
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outStream (lastRecordingFile.createOutputStream());
        
        if (outStream != nullptr)
        {
            auto numChannels = static_cast<unsigned int>(trimmedBuffer.getNumChannels());
            juce::StringPairArray metadata;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wavFormat.createWriterFor (outStream.get(), recordSampleRate, numChannels, 24, metadata, 0));

            if (writer != nullptr)
            {
                outStream.release();
                writer->writeFromAudioSampleBuffer (trimmedBuffer, 0, trimmedBuffer.getNumSamples());
                writer->flush(); // Ensure data is written to disk
                DBG("WAV file written successfully");
            }
            else
            {
                DBG("ERROR: Failed to create WAV writer!");
            }
            
            // Explicitly destroy the writer to close the file
            writer.reset();
        }
        else
        {
            DBG("ERROR: Failed to create output stream!");
        }

        // Small delay to ensure file is fully written and closed
        juce::Thread::sleep(10);

        if (lastRecordingFile.existsAsFile() && lastRecordingFile.getSize() > 0)
        {
            DBG("WAV file created successfully, size: " + juce::String(lastRecordingFile.getSize()) + " bytes");
            originalRecordingFile = lastRecordingFile; // Save as the original recording
            loadSampleFromFile (lastRecordingFile);
        }
        else
        {
            DBG("ERROR: WAV file does not exist or is empty!");
            clearLoadedSample();
        }
    }
    else
    {
        DBG("No audio recorded (recordPosition = 0)");
        clearLoadedSample();
    }

    recordPosition = 0;
}

void StaticCurrentsPluginAudioProcessor::loadSampleFromFile (const juce::File& file)
{
    DBG("loadSampleFromFile called: " + file.getFullPathName());
    DBG("File exists: " + juce::String(file.existsAsFile()) + ", Size: " + juce::String(file.getSize()));
    
    clearLoadedSample();
    auto* reader = formatManager.createReaderFor (file);
    
    if (reader != nullptr)
    {
        DBG("Reader created successfully!");
        DBG("Sample rate: " + juce::String(reader->sampleRate) + 
            ", Length: " + juce::String(reader->lengthInSamples) + 
            ", Channels: " + juce::String(reader->numChannels));
        
        // Create SamplerSound with stereo buffer
        // Maps to all MIDI notes (0-127) with max velocity range
        juce::BigInteger allNotes;
        allNotes.setRange (0, 128, true);
        
        sampler.clearSounds();
        sampler.addSound (new juce::SamplerSound ("Sample",
                                                   *reader,
                                                   allNotes,
                                                   60,   // root note (middle C)
                                                   0.0,  // no attack envelope (play full sample)
                                                   0.0,  // no release envelope (play full sample)
                                                   60.0  // max sample length in seconds
                                                   ));

        if (reader->sampleRate > 0.0)
        {
            float length = static_cast<float>(reader->lengthInSamples) / static_cast<float>(reader->sampleRate);
            sampleLength.store(length);
            DBG("Sample loaded! Length: " + juce::String(length, 3) + " seconds");
            DBG("Sampler now has " + juce::String(sampler.getNumSounds()) + " sounds loaded");
        }
        
        delete reader;
    }
    else
    {
        DBG("ERROR: Failed to create reader for file!");
    }
}

juce::File StaticCurrentsPluginAudioProcessor::createRecordingTempFile() const
{
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto timestamp = juce::String (juce::Time::getCurrentTime().toMilliseconds());
    return tempDir.getChildFile ("StaticCurrentsPlugin_recording_" + timestamp + ".wav");
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StaticCurrentsPluginAudioProcessor();
}

void StaticCurrentsPluginAudioProcessor::exportProcessedSample(const juce::File& outputFile)
{
    // Get the sample from the sampler
    if (sampler.getNumSounds() == 0)
        return;
    
    auto* samplerSound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(0).get());
    if (samplerSound == nullptr)
        return;
    
    auto* audioData = samplerSound->getAudioData();
    int numChannels = audioData->getNumChannels();
    int numSamples = audioData->getNumSamples();
    
    if (numSamples == 0)
        return;
    
    // Create a copy of the audio data to process
    juce::AudioBuffer<float> processedBuffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        processedBuffer.copyFrom(ch, 0, *audioData, ch, 0, numSamples);
    
    // Apply the same effects chain as in processBlock (if not bypassed)
    if (!bypass.load())
    {
        // 1. Saturation
        float satMix = juce::jlimit (0.0f, 1.0f, saturation.load());
        int satType = static_cast<int>(saturationType.load());
        int profile = static_cast<int>(profileType.load());

        // Profile scaling removed - presets only set parameter values, no runtime scaling
        if (satMix > 0.0f)
        {
            float tubeDriveVal = tubeDrive.load();
            float tubeWarmthVal = tubeWarmth.load();
            float tubeBiasVal = tubeBias.load();
            float tubeOutVal = tubeOutput.load();

            float transistorDriveVal = transistorDrive.load();
            float transistorBiteVal = transistorBite.load();
            float transistorClipVal = transistorClip.load();
            float transistorOutVal = transistorOutput.load();

            float tapeDriveVal = tapeDrive.load();
            float tapeWowVal = tapeWow.load();
            float tapeHissVal = tapeHiss.load();
            float tapeOutVal = tapeOutput.load();

            float diodeDriveVal = diodeDrive.load();
            float diodeAsymVal = diodeAsym.load();
            float diodeClipVal = diodeClip.load();
            float diodeOutVal = diodeOutput.load();

            float fuzzDriveVal = fuzzDrive.load();
            float fuzzGateVal = fuzzGate.load();
            float fuzzToneVal = fuzzTone.load();
            float fuzzOutVal = fuzzOutput.load();

            float bitDepthVal = bitDepth.load();
            float bitRateVal = bitRate.load();
            float bitMixVal = bitMix.load();
            float bitOutVal = bitOutput.load();

            const float wowRate = 0.2f + (tapeWowVal * 2.0f);
            const float wowInc = static_cast<float>((juce::MathConstants<double>::twoPi * wowRate) / currentSampleRate);
            double wowPhase = 0.0;
            float fuzzStateL = 0.0f;
            float fuzzStateR = 0.0f;
            int crushCounterL = 0;
            int crushCounterR = 0;
            float crushHoldL = 0.0f;
            float crushHoldR = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = processedBuffer.getWritePointer(ch);
                int& crushCounter = (ch == 0) ? crushCounterL : crushCounterR;
                float& crushHold = (ch == 0) ? crushHoldL : crushHoldR;
                float& fuzzState = (ch == 0) ? fuzzStateL : fuzzStateR;

                for (int i = 0; i < numSamples; ++i)
                {
                    float dry = data[i];
                    float processed = dry;

                    float tubeWeight = (tubeDriveVal / 10.0f) * 0.65f
                                     + tubeWarmthVal * 0.18f
                                     + std::abs (tubeBiasVal) * 0.07f
                                     + std::abs (tubeOutVal - 1.0f) * 0.10f;
                    float transistorWeight = (transistorDriveVal / 10.0f) * 0.55f
                                           + transistorBiteVal * 0.22f
                                           + transistorClipVal * 0.13f
                                           + std::abs (transistorOutVal - 1.0f) * 0.10f;
                    float tapeWeight = (tapeDriveVal / 10.0f) * 0.55f
                                     + tapeWowVal * 0.18f
                                     + tapeHissVal * 0.17f
                                     + std::abs (tapeOutVal - 1.0f) * 0.10f;
                    float diodeWeight = (diodeDriveVal / 10.0f) * 0.55f
                                      + diodeAsymVal * 0.18f
                                      + diodeClipVal * 0.17f
                                      + std::abs (diodeOutVal - 1.0f) * 0.10f;
                    float fuzzWeight = (fuzzDriveVal / 10.0f) * 0.55f
                                     + fuzzGateVal * 0.15f
                                     + (1.0f - fuzzToneVal) * 0.20f
                                     + std::abs (fuzzOutVal - 1.0f) * 0.10f;
                    float depthWeight = (16.0f - bitDepthVal) / 14.0f;
                    float rateWeight = (bitRateVal - 1.0f) / 15.0f;
                    float bitWeight = bitMixVal * 0.55f
                                    + depthWeight * 0.2f
                                    + rateWeight * 0.15f
                                    + std::abs (bitOutVal - 1.0f) * 0.10f;

                    tubeWeight = juce::jlimit (0.0f, 1.0f, tubeWeight);
                    transistorWeight = juce::jlimit (0.0f, 1.0f, transistorWeight);
                    tapeWeight = juce::jlimit (0.0f, 1.0f, tapeWeight);
                    diodeWeight = juce::jlimit (0.0f, 1.0f, diodeWeight);
                    fuzzWeight = juce::jlimit (0.0f, 1.0f, fuzzWeight);
                    bitWeight = juce::jlimit (0.0f, 1.0f, bitWeight);

                    const float focusBoost = 1.1f;
                    if (satType == 1) tubeWeight *= focusBoost;
                    if (satType == 2) transistorWeight *= focusBoost;
                    if (satType == 3) tapeWeight *= focusBoost;
                    if (satType == 4) diodeWeight *= focusBoost;
                    if (satType == 5) fuzzWeight *= focusBoost;
                    if (satType == 6) bitWeight *= focusBoost;

                    // Input attenuation for hot saturation modes (like real analog gear)
                    float preAtten = 0.7f;
                    float dryScaled = dry * preAtten;

                    float tubeDrive = 1.0f + tubeDriveVal * 0.6f;  // Increased for more extreme effect (max 7x)
                    float tubeBias = tubeBiasVal * 0.5f;  // Doubled bias shift for more pronounced effect
                    float tubeWarm = (0.6f + tubeWarmthVal * 2.0f);  // Increased warmth effect
                    float tubeDriven = (dryScaled + tubeBias) * tubeDrive;
                    float tubeEven = std::abs (tubeDriven) * tubeDriven * (0.25f * tubeWarmthVal);  // More even harmonics
                    float tubeSat = std::tanh ((tubeDriven + tubeEven) * tubeWarm);
                    float tubeComp = 1.0f / (1.0f + std::abs (tubeSat) * 0.6f);
                    float tubeOut = tubeSat * tubeComp * tubeOutVal / preAtten;  // Compensate attenuation

                    float transDrive = 1.0f + transistorDriveVal * 0.6f;  // Increased for more extreme effect (max 7x)
                    float transBite = juce::jlimit (0.0f, 1.0f, transistorBiteVal);
                    float transClip = 0.9f - (transistorClipVal * 0.7f);
                    float transDriven = dryScaled * transDrive;
                    float transClipped = juce::jlimit (-transClip, transClip, transDriven);
                    float transSoft = std::tanh (transClipped * (1.0f + transBite * 4.0f));  // More aggressive bite effect
                    float transHard = transClipped / transClip;
                    float transSat = juce::jlimit (-1.0f, 1.0f, transSoft * (1.0f - transBite) + transHard * transBite);
                    
                    // Add crossover distortion (transistor characteristic)
                    float crossover = transSat * 0.05f * (1.0f - std::abs(transSat));  // More pronounced crossover
                    transSat += crossover * transBite;
                    
                    float transOut = transSat * transistorOutVal / preAtten;  // Compensate attenuation

                    wowPhase += wowInc;
                    if (wowPhase > juce::MathConstants<double>::twoPi)
                        wowPhase -= juce::MathConstants<double>::twoPi;

                    float wowMod = 1.0f + std::sin (static_cast<float>(wowPhase)) * tapeWowVal * 0.05f;  // More wow/flutter
                    float tapeDrive = 1.0f + tapeDriveVal * 1.0f * wowMod;  // Increased drive (max 11x)
                    float tapeDriven = dryScaled * tapeDrive;
                    float tapeComp = tapeDriven / (1.0f + std::abs (tapeDriven) * 0.7f);
                    float tapeSat = std::tanh (tapeComp * 1.12f);
                    float tapeLoss = 1.0f - tapeHissVal * 0.6f;  // More pronounced hiss/loss
                    float tapeOut = tapeSat * tapeLoss * tapeOutVal / preAtten;  // Compensate attenuation

                    float diodeDrive = 1.0f + diodeDriveVal * 0.7f;  // Increased for more extreme effect (max 8x) (max 4.5x instead of 13x)
                    float diodeAsym = juce::jlimit (0.0f, 1.0f, diodeAsymVal);
                    float diodeClip = 0.95f - (diodeClipVal * 0.75f);
                    float diodeDriven = dryScaled * diodeDrive;
                    float diodeClipped = juce::jlimit (-diodeClip, diodeClip, diodeDriven);
                    float diodeRect = (1.0f - diodeAsym) * diodeClipped + diodeAsym * std::abs (diodeClipped);
                    
                    // Forward voltage drop simulation (0.6V diode characteristic)
                    float fwdDrop = 0.6f / 10.0f;  // Normalized
                    if (diodeRect > fwdDrop)
                        diodeRect = diodeRect - fwdDrop;
                    else if (diodeRect < -fwdDrop)
                        diodeRect = diodeRect + fwdDrop;
                    else
                        diodeRect = 0.0f;
                    
                    float diodeSat = std::tanh (diodeRect * (1.2f + diodeClipVal * 2.0f));  // More harmonic distortion
                    float diodeOut = diodeSat * diodeOutVal / preAtten;  // Compensate attenuation

                    float fuzzDrive = 1.0f + fuzzDriveVal * 0.7f;  // Increased for more extreme effect (max 8x) (max 5x instead of 17x) - CRITICAL FIX
                    float fuzzGate = fuzzGateVal * 0.12f;  // More aggressive gating
                    float fuzzTone = juce::jlimit (0.0f, 1.0f, fuzzToneVal);
                    float fuzzDriven = dryScaled * fuzzDrive;
                    float fuzzed = juce::jlimit (-1.0f, 1.0f, fuzzDriven);
                    if (std::abs (fuzzed) < fuzzGate)
                        fuzzed *= std::abs (fuzzed) / juce::jmax (0.001f, fuzzGate);
                    
                    // Add octave-up effect (fuzz characteristic - frequency doubling)
                    float octaveUp = std::abs(fuzzed) * fuzzed * 0.25f;  // More octave-up harmonics
                    fuzzed = fuzzed * 0.75f + octaveUp;  // Adjusted mix for stronger effect
                    
                    float fuzzAlpha = 0.08f + (1.0f - fuzzTone) * 0.6f;  // More extreme tone shaping
                    fuzzState += fuzzAlpha * (fuzzed - fuzzState);
                    float fuzzOut = fuzzState * fuzzOutVal / preAtten;  // Compensate attenuation

                    int bits = juce::jlimit (2, 16, static_cast<int>(std::round (bitDepthVal)));
                    int rate = juce::jlimit (1, 16, static_cast<int>(std::round (bitRateVal)));
                    float step = 2.0f / static_cast<float> (1 << bits);

                    float crushSample = dryScaled;
                    if (rate > 1)
                    {
                        if (crushCounter <= 0)
                        {
                            crushHold = crushSample;
                            crushCounter = rate - 1;
                        }
                        else
                        {
                            --crushCounter;
                            crushSample = crushHold;
                        }
                    }

                    float quant = std::floor (crushSample / step) * step;
                    float bitWet = juce::jlimit (0.0f, 1.0f, bitMixVal);
                    float bitOut = (dryScaled * (1.0f - bitWet) + quant * bitWet) * bitOutVal / preAtten;  // Compensate attenuation

                    float weightSum = tubeWeight + transistorWeight + tapeWeight + diodeWeight + fuzzWeight + bitWeight;
                    if (weightSum < 0.0001f)
                    {
                        processed = dry;
                    }
                    else
                    {
                        processed = (tubeOut * tubeWeight
                                     + transOut * transistorWeight
                                     + tapeOut * tapeWeight
                                     + diodeOut * diodeWeight
                                     + fuzzOut * fuzzWeight
                                     + bitOut * bitWeight) / weightSum;
                    }

                    data[i] = dry * (1.0f - satMix) + processed * satMix;
                }
            }
        }
        
        // 2. 6-Band Parametric EQ - Create fresh filters for offline processing
        juce::IIRFilter hpfL_offline, hpfR_offline;
        juce::IIRFilter peak1L_offline, peak1R_offline;
        juce::IIRFilter peak2L_offline, peak2R_offline;
        juce::IIRFilter peak3L_offline, peak3R_offline;
        juce::IIRFilter peak4L_offline, peak4R_offline;
        juce::IIRFilter lpfL_offline, lpfR_offline;
        
        // HPF
        auto hpf_freq = hpfFreq.load();
        auto hpfCoeffs = juce::IIRCoefficients::makeHighPass(currentSampleRate, hpf_freq);
        hpfL_offline.setCoefficients(hpfCoeffs);
        hpfR_offline.setCoefficients(hpfCoeffs);
        
        // Peak 1
        auto p1_freq = peak1Freq.load();
        auto p1_gain = peak1Gain.load();
        auto p1_q = peak1Q.load();
        auto peak1Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p1_freq, p1_q, juce::Decibels::decibelsToGain(p1_gain));
        peak1L_offline.setCoefficients(peak1Coeffs);
        peak1R_offline.setCoefficients(peak1Coeffs);
        
        // Peak 2
        auto p2_freq = peak2Freq.load();
        auto p2_gain = peak2Gain.load();
        auto p2_q = peak2Q.load();
        auto peak2Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p2_freq, p2_q, juce::Decibels::decibelsToGain(p2_gain));
        peak2L_offline.setCoefficients(peak2Coeffs);
        peak2R_offline.setCoefficients(peak2Coeffs);
        
        // Peak 3
        auto p3_freq = peak3Freq.load();
        auto p3_gain = peak3Gain.load();
        auto p3_q = peak3Q.load();
        auto peak3Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p3_freq, p3_q, juce::Decibels::decibelsToGain(p3_gain));
        peak3L_offline.setCoefficients(peak3Coeffs);
        peak3R_offline.setCoefficients(peak3Coeffs);
        
        // Peak 4
        auto p4_freq = peak4Freq.load();
        auto p4_gain = peak4Gain.load();
        auto p4_q = peak4Q.load();
        auto peak4Coeffs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, p4_freq, p4_q, juce::Decibels::decibelsToGain(p4_gain));
        peak4L_offline.setCoefficients(peak4Coeffs);
        peak4R_offline.setCoefficients(peak4Coeffs);
        
        // LPF
        auto lpf_freq = lpfFreq.load();
        auto lpfCoeffs = juce::IIRCoefficients::makeLowPass(currentSampleRate, lpf_freq);
        lpfL_offline.setCoefficients(lpfCoeffs);
        lpfR_offline.setCoefficients(lpfCoeffs);
        
        // Apply all EQ bands in series
        if (numChannels > 0)
        {
            hpfL_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
            peak1L_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
            peak2L_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
            peak3L_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
            peak4L_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
            lpfL_offline.processSamples(processedBuffer.getWritePointer(0), numSamples);
        }
        if (numChannels > 1)
        {
            hpfR_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
            peak1R_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
            peak2R_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
            peak3R_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
            peak4R_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
            lpfR_offline.processSamples(processedBuffer.getWritePointer(1), numSamples);
        }
        
        // 3. FET-Style Compression (matches real-time processing)
        float threshold = compThresh.load();
        float ratio = compRatio.load();
        float attackTime = compAttack.load();
        float releaseTime = compRelease.load();
        float makeup = compMakeup.load();
        float envelope = 0.0f;
        
        // FET compressor time constants
        float attackCoeff = 1.0f - std::exp(-1.0f / (attackTime * static_cast<float>(currentSampleRate) * 0.5f));
        float releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * static_cast<float>(currentSampleRate)));
        
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax(peak, std::abs(processedBuffer.getSample(ch, i)));
            
            float peakDb = juce::Decibels::gainToDecibels(peak + 0.0001f);
            float gainReduction = 0.0f;
            
            // Soft-knee compression
            if (peakDb > threshold - compKneeWidth / 2.0f)
            {
                if (peakDb < threshold + compKneeWidth / 2.0f)
                {
                    float kneeInput = peakDb - threshold + compKneeWidth / 2.0f;
                    float kneeSquared = kneeInput * kneeInput;
                    gainReduction = kneeSquared / (2.0f * compKneeWidth) * (1.0f - 1.0f / ratio);
                }
                else
                {
                    float excess = peakDb - threshold;
                    gainReduction = excess * (1.0f - 1.0f / ratio);
                    
                    if (gainReduction > 10.0f)
                    {
                        float satAmount = (gainReduction - 10.0f) * 0.05f;
                        gainReduction += satAmount * satAmount;
                    }
                }
            }
            
            float coeff = (gainReduction > envelope) ? attackCoeff : releaseCoeff;
            envelope += (gainReduction - envelope) * coeff;
            
            float compGain = juce::Decibels::decibelsToGain(-envelope + makeup);
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float sample = processedBuffer.getSample(ch, i) * compGain;
                
                if (envelope > 3.0f)
                {
                    float colorAmount = juce::jmin(envelope * 0.008f, 0.08f);
                    sample = sample + colorAmount * std::tanh(sample * 3.0f) * 0.2f;
                }
                
                processedBuffer.setSample(ch, i, sample);
            }
        }
    }
    
    // Apply gain
    float currentGain = gain.load();
    processedBuffer.applyGain(currentGain);
    
    // Write to file
    outputFile.deleteFile();
    std::unique_ptr<juce::OutputStream> outputStream (outputFile.createOutputStream());
    
    if (outputStream != nullptr)
    {
        std::unique_ptr<juce::AudioFormat> format;
        int bitDepth = 24;
        
        // Detect format from file extension
        if (outputFile.hasFileExtension(".wav"))
        {
            format = std::make_unique<juce::WavAudioFormat>();
            // Check filename for bit depth hint
            if (outputFile.getFileNameWithoutExtension().containsIgnoreCase("16"))
                bitDepth = 16;
            else
                bitDepth = 24;
        }
        else if (outputFile.hasFileExtension(".mp3"))
        {
            #if JUCE_USE_LAME_AUDIO_FORMAT
            format = std::make_unique<juce::LAMEEncoderAudioFormat>(outputFile);
            bitDepth = 16;
            #else
            // MP3 encoding not available - fall back to WAV
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "MP3 Not Available",
                "MP3 encoding requires LAME library. Saving as WAV instead.");
            format = std::make_unique<juce::WavAudioFormat>();
            bitDepth = 24;
            #endif
        }
        else if (outputFile.hasFileExtension(".ogg"))
        {
            format = std::make_unique<juce::OggVorbisAudioFormat>();
            bitDepth = 16; // OGG uses quality setting, not bit depth
        }
        else if (outputFile.hasFileExtension(".flac"))
        {
            format = std::make_unique<juce::FlacAudioFormat>();
            bitDepth = 24;
        }
        else
        {
            // Default to WAV
            format = std::make_unique<juce::WavAudioFormat>();
            bitDepth = 24;
        }
        
        auto numChannels = static_cast<unsigned int>(processedBuffer.getNumChannels());
        auto channelLayout = (numChannels == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        
        auto options = juce::AudioFormatWriterOptions{}
                   .withSampleRate (currentSampleRate)
                   .withChannelLayout (channelLayout)
                   .withBitsPerSample (bitDepth);
        auto writer = format->createWriterFor (outputStream, options);
        
        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer(processedBuffer, 0, numSamples);
        }
    }
}

void StaticCurrentsPluginAudioProcessor::jumbleSample()
{
    // Get the current sample from the sampler
    if (sampler.getNumSounds() == 0)
    {
        DBG("No sample loaded to jumble!");
        return;
    }
    
    auto* samplerSound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(0).get());
    if (samplerSound == nullptr)
        return;
    
    auto* audioData = samplerSound->getAudioData();
    int numChannels = audioData->getNumChannels();
    int numSamples = audioData->getNumSamples();
    
    if (numSamples < 1000)  // Need at least some minimum length
    {
        DBG("Sample too short to jumble!");
        return;
    }
    
    // Calculate crossfade length (0.005 seconds - just enough to prevent clicks)
    int crossfadeLength = static_cast<int>(0.005 * currentSampleRate);
    
    // Generate many rapid cuts (between 40 and 100 for fast gibberish)
    juce::Random random;
    int numCuts = random.nextInt(juce::Range<int>(40, 101));
    
    // Calculate slice length
    int sliceLength = numSamples / numCuts;
    
    // Create slices
    struct Slice
    {
        int startSample;
        int length;
        float speedFactor;  // 0.5 = half speed, 1.0 = normal, 2.0 = double speed
        bool reverse;
    };
    
    std::vector<Slice> slices;
    int currentPos = 0;
    
    for (int i = 0; i < numCuts && currentPos < numSamples; ++i)
    {
        Slice slice;
        slice.startSample = currentPos;
        
        // Vary slice length by ±20% (tighter variation for consistent rapid cuts)
        int variance = static_cast<int>(sliceLength * 0.2f);
        slice.length = sliceLength + random.nextInt(juce::Range<int>(-variance, variance + 1));
        slice.length = juce::jlimit(crossfadeLength * 2, numSamples - currentPos, slice.length);
        
        // Random speed: 30% normal, 35% slightly slow (0.7-0.95x), 35% slightly fast (1.05-1.5x)
        // Tighter range for more intelligible but still garbled speech
        float speedChoice = random.nextFloat();
        if (speedChoice < 0.3f)
            slice.speedFactor = 1.0f;  // Normal speed
        else if (speedChoice < 0.65f)
            slice.speedFactor = 0.7f + random.nextFloat() * 0.25f;  // Slightly slow (0.7-0.95x)
        else
            slice.speedFactor = 1.05f + random.nextFloat() * 0.45f;  // Slightly fast (1.05-1.5x)
        
        // 40% chance of reverse (more chaos)
        slice.reverse = random.nextFloat() < 0.4f;
        
        slices.push_back(slice);
        currentPos += slice.length;
    }
    
    // Shuffle the slices (Fisher-Yates shuffle with JUCE Random)
    juce::Random rng;
    for (int i = slices.size() - 1; i > 0; --i)
    {
        int j = rng.nextInt(i + 1);
        std::swap(slices[i], slices[j]);
    }
    
    // Estimate new buffer size (accounting for speed changes)
    int estimatedLength = 0;
    for (const auto& slice : slices)
    {
        estimatedLength += static_cast<int>(slice.length / slice.speedFactor) + crossfadeLength;
    }
    
    // Create jumbled buffer
    juce::AudioBuffer<float> jumbledBuffer(numChannels, estimatedLength);
    jumbledBuffer.clear();
    
    int writePos = 0;
    
    for (size_t i = 0; i < slices.size(); ++i)
    {
        const auto& slice = slices[i];
        
        // Calculate resampled length
        int resampledLength = static_cast<int>(slice.length / slice.speedFactor);
        
        // Create temporary buffer for this slice
        juce::AudioBuffer<float> sliceBuffer(numChannels, resampledLength);
        
        // Copy and process slice
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Resample the slice
            for (int sampleIdx = 0; sampleIdx < resampledLength; ++sampleIdx)
            {
                float sourcePos = sampleIdx * slice.speedFactor;
                int sourceSample = slice.startSample + static_cast<int>(sourcePos);
                
                // Linear interpolation
                if (sourceSample < numSamples - 1)
                {
                    float frac = sourcePos - std::floor(sourcePos);
                    float sample1 = audioData->getSample(ch, sourceSample);
                    float sample2 = audioData->getSample(ch, sourceSample + 1);
                    float interpolated = sample1 + frac * (sample2 - sample1);
                    
                    int writeIdx = slice.reverse ? (resampledLength - 1 - sampleIdx) : sampleIdx;
                    sliceBuffer.setSample(ch, writeIdx, interpolated);
                }
            }
        }
        
        // Add slice to jumbled buffer with crossfade
        int copyLength = juce::jmin(resampledLength, estimatedLength - writePos);
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // If not first slice, apply crossfade
            if (i > 0 && writePos >= crossfadeLength)
            {
                // Fade out previous slice
                for (int j = 0; j < crossfadeLength && (writePos - crossfadeLength + j) < estimatedLength; ++j)
                {
                    float fadeOut = 1.0f - (j / static_cast<float>(crossfadeLength));
                    float existingSample = jumbledBuffer.getSample(ch, writePos - crossfadeLength + j);
                    jumbledBuffer.setSample(ch, writePos - crossfadeLength + j, existingSample * fadeOut);
                }
                
                // Fade in current slice
                for (int j = 0; j < crossfadeLength && j < copyLength; ++j)
                {
                    float fadeIn = j / static_cast<float>(crossfadeLength);
                    float newSample = sliceBuffer.getSample(ch, j);
                    float existingSample = jumbledBuffer.getSample(ch, writePos - crossfadeLength + j);
                    jumbledBuffer.setSample(ch, writePos - crossfadeLength + j, existingSample + newSample * fadeIn);
                }
                
                // Copy rest of slice without crossfade
                for (int j = crossfadeLength; j < copyLength; ++j)
                {
                    if (writePos + j - crossfadeLength < estimatedLength)
                        jumbledBuffer.setSample(ch, writePos + j - crossfadeLength, sliceBuffer.getSample(ch, j));
                }
            }
            else
            {
                // First slice or no room for crossfade - just copy
                for (int j = 0; j < copyLength && (writePos + j) < estimatedLength; ++j)
                {
                    jumbledBuffer.setSample(ch, writePos + j, sliceBuffer.getSample(ch, j));
                }
            }
        }
        
        writePos += (i > 0) ? (copyLength - crossfadeLength) : copyLength;
    }
    
    // Trim to actual length
    int actualLength = juce::jmin(writePos, estimatedLength);
    juce::AudioBuffer<float> finalBuffer(numChannels, actualLength);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        finalBuffer.copyFrom(ch, 0, jumbledBuffer, ch, 0, actualLength);
    }
    
    // Save jumbled sample to temporary file
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("StaticCurrentsPlugin_jumbled_" + 
                                  juce::String(juce::Time::getCurrentTime().toMilliseconds()) + 
                                  ".wav");
    
    // Write to file
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(new juce::FileOutputStream(tempFile));
    
    if (outputStream->openedOk())
    {
        outputStream->setPosition(0);
        outputStream->truncate();
        
        auto numChannels = static_cast<unsigned int>(finalBuffer.getNumChannels());
        juce::StringPairArray metadata;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outputStream.get(), currentSampleRate, numChannels, 24, metadata, 0));
        
        if (writer != nullptr)
        {
            outputStream.release();  // Writer takes ownership
            writer->writeFromAudioSampleBuffer(finalBuffer, 0, actualLength);
            writer->flush();
            
            // Load jumbled sample back into sampler
            loadSampleFromFile(tempFile);
            DBG("Sample jumbled successfully! " + juce::String(slices.size()) + " slices, final length: " + juce::String(actualLength / currentSampleRate, 2) + "s");
        }
    }
}

