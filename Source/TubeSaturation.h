/*
  ==============================================================================

    TubeSaturation.h
    
    Tube-style saturation with warmth, bias control, and soft-knee compression.
    Implements smooth even-order harmonics via asymmetric waveshaping.
    
    Signal Flow:
    Input → Upsample 2x → Pre-emphasis → Warmth (low-boost) → Drive → 
          → Bias + Waveshaper → De-emphasis → Downsample 2x → Output Gain
    
    Uses manual 2x oversampling with basic linear interpolation to minimize 
    aliasing from nonlinear processing.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Tube saturation processor with frequency-dependent saturation and asymmetric
    harmonic generation. Designed to mimic the warm, smooth compression of 
    vacuum tube circuits.
*/
class TubeSaturation
{
public:
    TubeSaturation() = default;
    ~TubeSaturation() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels)
    {
        currentSampleRate = sampleRate;
        oversampledRate = sampleRate * 2.0;
        
        // Allocate oversampled buffer (2x the size for 2x oversampling)
        oversampledBuffer.setSize(numChannels, samplesPerBlock * 2);
        
        // Initialize filters at oversampled rate
        updateFilterCoefficients();
        
        reset();
    }

    void reset()
    {
        oversampledBuffer.clear();
        
        // Reset filter states for both channels
        for (int ch = 0; ch < 2; ++ch)
        {
            preEmphZ1[ch] = 0.0f;
            preEmphZ2[ch] = 0.0f;
            deEmphZ1[ch] = 0.0f;
            deEmphZ2[ch] = 0.0f;
            warmthZ1[ch] = 0.0f;
            warmthZ2[ch] = 0.0f;
        }
    }

    //==============================================================================
    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        
        if (numChannels == 0 || numSamples == 0)
            return;
        
        // Process each channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer(ch);
            auto* oversampledData = oversampledBuffer.getWritePointer(ch);
            
            // Step 1: Upsample to 2x (linear interpolation)
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = channelData[i];
                float nextSample = (i < numSamples - 1) ? channelData[i + 1] : sample;
                
                oversampledData[i * 2] = sample;
                oversampledData[i * 2 + 1] = (sample + nextSample) * 0.5f;
            }
            
            // Step 2: Process at oversampled rate
            processOversampledChannel(oversampledData, numSamples * 2, ch);
            
            // Step 3: Downsample back to original rate (simple decimation)
            for (int i = 0; i < numSamples; ++i)
            {
                channelData[i] = oversampledData[i * 2];
            }
        }
    }

    //==============================================================================
    // Parameter setters
    void setDrive(float newDrive)
    {
        drive = juce::jlimit(0.1f, 10.0f, newDrive);
    }

    void setWarmth(float warmthDb)
    {
        warmth = juce::jlimit(-12.0f, 6.0f, warmthDb);
        updateWarmthFilter();
    }

    void setBias(float newBias)
    {
        bias = juce::jlimit(-1.0f, 1.0f, newBias);
    }

    void setOutputGain(float gainDb)
    {
        outputGain = juce::Decibels::decibelsToGain(juce::jlimit(-12.0f, 12.0f, gainDb));
    }

private:
    //==============================================================================
    void processOversampledChannel(float* data, int numSamples, int channel)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            
            // Stage 1: Pre-emphasis (gentle high-cut before saturation)
            sample = applyBiquad(sample, preEmphB0, preEmphB1, preEmphB2, 
                                preEmphA1, preEmphA2,
                                preEmphZ1[channel], preEmphZ2[channel]);
            
            // Stage 2: Warmth (low-frequency boost)
            sample = applyBiquad(sample, warmthB0, warmthB1, warmthB2,
                                warmthA1, warmthA2,
                                warmthZ1[channel], warmthZ2[channel]);
            
            // Stage 3: Drive
            sample *= drive;
            
            // Stage 4: Bias + Waveshaping (tube-like asymmetry)
            // Adding bias before saturation creates even-order harmonics
            float biasedSample = sample + (bias * sample * sample * 0.5f);
            
            // Soft saturation using tanh (smooth tube-like curve)
            float saturated = std::tanh(biasedSample);
            
            // Remove DC offset from bias
            if (std::abs(bias) > 0.001f)
                saturated -= std::tanh(bias * 0.25f);
            
            // Stage 5: De-emphasis (restore high-frequency balance)
            saturated = applyBiquad(saturated, deEmphB0, deEmphB1, deEmphB2,
                                   deEmphA1, deEmphA2,
                                   deEmphZ1[channel], deEmphZ2[channel]);
            
            // Stage 6: Output gain
            saturated *= outputGain;
            
            // Safety: sanitize output
            saturated = sanitize(saturated);
            
            data[i] = saturated;
        }
    }

    //==============================================================================
    // Biquad filter implementation (Direct Form II)
    inline float applyBiquad(float input, 
                            float b0, float b1, float b2,
                            float a1, float a2,
                            float& z1, float& z2)
    {
        float output = input * b0 + z1;
        z1 = input * b1 - a1 * output + z2;
        z2 = input * b2 - a2 * output;
        return output;
    }

    // Update filter coefficients
    void updateFilterCoefficients()
    {
        // Pre-emphasis: high-shelf cut at 8kHz, -3dB (reduces aliasing)
        makeHighShelf(8000.0f, 0.707f, -3.0f, oversampledRate,
                     preEmphB0, preEmphB1, preEmphB2, preEmphA1, preEmphA2);
        
        // De-emphasis: reciprocal high-shelf
        makeHighShelf(8000.0f, 0.707f, 3.0f, oversampledRate,
                     deEmphB0, deEmphB1, deEmphB2, deEmphA1, deEmphA2);
        
        // Warmth filter will be updated when parameter changes
        updateWarmthFilter();
    }

    void updateWarmthFilter()
    {
        // Warmth: low-shelf at 200Hz, variable gain
        makeLowShelf(200.0f, 0.707f, warmth, oversampledRate,
                    warmthB0, warmthB1, warmthB2, warmthA1, warmthA2);
    }

    // High-shelf filter designer
    void makeHighShelf(float freq, float Q, float gainDb, double sampleRate,
                      float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float>(sampleRate);
        float cosW0 = std::cos(w0);
        float sinW0 = std::sin(w0);
        float alpha = sinW0 / (2.0f * Q);
        
        float sqrtA = std::sqrt(A);
        float b0Unnorm = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
        float b1Unnorm = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
        float b2Unnorm = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
        float a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
        float a1Unnorm = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
        float a2Unnorm = (A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
        
        // Normalize
        b0 = b0Unnorm / a0;
        b1 = b1Unnorm / a0;
        b2 = b2Unnorm / a0;
        a1 = a1Unnorm / a0;
        a2 = a2Unnorm / a0;
    }

    // Low-shelf filter designer
    void makeLowShelf(float freq, float Q, float gainDb, double sampleRate,
                     float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float>(sampleRate);
        float cosW0 = std::cos(w0);
        float sinW0 = std::sin(w0);
        float alpha = sinW0 / (2.0f * Q);
        
        float sqrtA = std::sqrt(A);
        float b0Unnorm = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
        float b1Unnorm = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
        float b2Unnorm = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
        float a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
        float a1Unnorm = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
        float a2Unnorm = (A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
        
        // Normalize
        b0 = b0Unnorm / a0;
        b1 = b1Unnorm / a0;
        b2 = b2Unnorm / a0;
        a1 = a1Unnorm / a0;
        a2 = a2Unnorm / a0;
    }

    // Sanitize output (denormal + NaN protection)
    inline float sanitize(float x)
    {
        if (!std::isfinite(x))
            return 0.0f;
        
        // Flush denormals to zero
        if (std::abs(x) < 1e-15f)
            return 0.0f;
        
        // Hard limit to ±1.0
        return juce::jlimit(-1.0f, 1.0f, x);
    }

    //==============================================================================
    // Oversampled processing buffer
    juce::AudioBuffer<float> oversampledBuffer;
    
    // Filter coefficients (at 2x sample rate)
    float preEmphB0 = 0.0f, preEmphB1 = 0.0f, preEmphB2 = 0.0f;
    float preEmphA1 = 0.0f, preEmphA2 = 0.0f;
    
    float deEmphB0 = 0.0f, deEmphB1 = 0.0f, deEmphB2 = 0.0f;
    float deEmphA1 = 0.0f, deEmphA2 = 0.0f;
    
    float warmthB0 = 0.0f, warmthB1 = 0.0f, warmthB2 = 0.0f;
    float warmthA1 = 0.0f, warmthA2 = 0.0f;
    
    // Filter states (z^-1, z^-2) for each channel
    float preEmphZ1[2] = {0.0f, 0.0f};
    float preEmphZ2[2] = {0.0f, 0.0f};
    float deEmphZ1[2] = {0.0f, 0.0f};
    float deEmphZ2[2] = {0.0f, 0.0f};
    float warmthZ1[2] = {0.0f, 0.0f};
    float warmthZ2[2] = {0.0f, 0.0f};
    
    // Parameters
    float drive = 1.0f;
    float warmth = 0.0f;
    float bias = 0.0f;
    float outputGain = 1.0f;
    
    double currentSampleRate = 44100.0;
    double oversampledRate = 88200.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubeSaturation)
};

