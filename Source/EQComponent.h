/*
  ==============================================================================

    EQComponent.h
    Created: 8 Feb 2026
    Parametric EQ visualization component

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class EQComponent : public juce::Component
{
public:
    EQComponent()
    {
        // Initialize with default flat response
        for (int i = 0; i < 6; ++i)
        {
            bandEnabled[i] = true;
        }
    }
    
    ~EQComponent() override {}
    
    std::function<void(int, float, float)> onBandDragged;
    std::function<void(int, float)> onQChanged;
    std::function<void(int, float)> onSlopeChanged;
    
    juce::Colour getBandColor(int bandIndex) const
    {
        switch (bandIndex)
        {
            case 0: return juce::Colour(0xff6080ff); // HPF - Blue
            case 1: return juce::Colour(0xff80ff60); // Peak 1 - Green
            case 2: return juce::Colour(0xffffd060); // Peak 2 - Yellow
            case 3: return juce::Colour(0xffff8060); // Peak 3 - Orange
            case 4: return juce::Colour(0xffff60a0); // Peak 4 - Pink
            case 5: return juce::Colour(0xffa060ff); // LPF - Purple
            default: return juce::Colours::white;
        }
    }
    
    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.fillAll (juce::Colour (0xff1a1a1a));
        
        // Grid
        g.setColour (juce::Colour (0xff2a2a2a));
        const int numHLines = 7;
        const int numVLines = 10;
        
        for (int i = 0; i <= numHLines; ++i)
        {
            float y = bounds.getHeight() * i / numHLines;
            g.drawLine (0, y, bounds.getWidth(), y, 1.0f);
        }
        
        for (int i = 0; i <= numVLines; ++i)
        {
            float x = bounds.getWidth() * i / numVLines;
            g.drawLine (x, 0, x, bounds.getHeight(), 1.0f);
        }
        
        // Center line (0 dB)
        g.setColour (juce::Colour (0xff3a3a3a));
        float centerY = bounds.getHeight() / 2.0f;
        g.drawLine (0, centerY, bounds.getWidth(), centerY, 2.0f);
        
        // Frequency markers
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        g.setColour (juce::Colours::white.withAlpha(0.6f));
        g.setFont (juce::FontOptions (10.0f));
        std::vector<float> markerFreqs = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
        std::vector<juce::String> markerLabels = {"20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"};
        
        for (size_t i = 0; i < markerFreqs.size(); ++i)
        {
            float normalizedFreq = std::log(markerFreqs[i] / minFreq) / std::log(maxFreq / minFreq);
            float x = bounds.getWidth() * normalizedFreq;
            
            // Draw tick mark
            g.setColour (juce::Colour (0xff4a4a4a));
            g.drawLine (x, 0, x, bounds.getHeight(), 1.5f);
            
            // Draw label
            g.setColour (juce::Colours::white.withAlpha(0.7f));
            g.drawText (markerLabels[i], x - 20, bounds.getHeight() - 15, 40, 12, juce::Justification::centred);
        }
        
        // Draw frequency response curve
        juce::Path responseCurve;
        const int numPoints = 300;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float proportion = i / (float)numPoints;
            float freq = minFreq * std::pow(maxFreq / minFreq, proportion);
            float magnitude = calculateMagnitudeForFrequency(freq);
            
            // Convert magnitude to pixel position with expanded range
            float x = bounds.getWidth() * proportion;
            float dbRange = 48.0f; // -24 to +24 dB visible range (centered at 0dB)
            float normalizedMag = (magnitude + 24.0f) / dbRange;
            float y = bounds.getHeight() * (1.0f - normalizedMag);
            
            if (i == 0)
                responseCurve.startNewSubPath(x, y);
            else
                responseCurve.lineTo(x, y);
        }
        
        // Draw the curve
        g.setColour (juce::Colour (0xff00aaff));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
        
        // Draw band markers
        g.setColour (juce::Colour (0xff888888));
        for (int i = 0; i < 6; ++i)
        {
            if (bandEnabled[i] && bandFreq[i] > 0.0f)
            {
                float normalizedFreq = std::log(bandFreq[i] / minFreq) / std::log(maxFreq / minFreq);
                float x = bounds.getWidth() * normalizedFreq;
                g.drawLine(x, 0, x, bounds.getHeight(), 1.0f);
                
                // Draw handle
                float magnitude = calculateMagnitudeForFrequency(bandFreq[i]);
                float normalizedMag = (magnitude + 24.0f) / 48.0f; // Match expanded range
                float y = bounds.getHeight() * (1.0f - normalizedMag);
                
                // Use distinct color for each band, brighten if hovered/dragged
                juce::Colour bandColor = getBandColor(i);
                if (i == hoveredBand || i == draggedBand)
                    g.setColour(bandColor.brighter(0.3f));
                else
                    g.setColour(bandColor);
                    
                g.fillEllipse(x - 5, y - 5, 10, 10);
                
                // Draw outer ring for better visibility
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.drawEllipse(x - 5, y - 5, 10, 10, 1.5f);
            }
        }
    }
    
    void resized() override
    {
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        // Highlight band handles on hover
        auto bounds = getLocalBounds().toFloat();
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        
        hoveredBand = -1;
        for (int i = 0; i < 6; ++i)
        {
            if (!bandEnabled[i] || bandFreq[i] <= 0.0f)
                continue;
                
            float normalizedFreq = std::log(bandFreq[i] / minFreq) / std::log(maxFreq / minFreq);
            float x = bounds.getWidth() * normalizedFreq;
            
            float magnitude = calculateMagnitudeForFrequency(bandFreq[i]);
            float normalizedMag = (magnitude + 24.0f) / 48.0f; // Match expanded range
            float y = bounds.getHeight() * (1.0f - normalizedMag);
            
            float distance = std::sqrt(std::pow(event.x - x, 2) + std::pow(event.y - y, 2));
            if (distance < 10.0f)
            {
                hoveredBand = i;
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                repaint();
                return;
            }
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (hoveredBand >= 0)
        {
            draggedBand = hoveredBand;
            dragStartFreq = bandFreq[draggedBand];
            dragStartGain = bandGain[draggedBand];
        }
    }
    
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (draggedBand < 0)
            return;
            
        auto bounds = getLocalBounds().toFloat();
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        
        // Calculate new frequency from X position
        float normalizedX = juce::jlimit(0.0f, 1.0f, event.x / bounds.getWidth());
        float newFreq = minFreq * std::pow(maxFreq / minFreq, normalizedX);
        
        // Calculate new gain from Y position (only for peak bands)
        float normalizedY = 1.0f - juce::jlimit(0.0f, 1.0f, event.y / bounds.getHeight());
        float newGain = (normalizedY * 48.0f) - 24.0f; // -24 to +24 dB with expanded range
        
        // Update the band
        if (draggedBand >= 1 && draggedBand <= 4) // Peak bands only
        {
            bandFreq[draggedBand] = newFreq;
            bandGain[draggedBand] = newGain;
            
            if (onBandDragged)
                onBandDragged(draggedBand, newFreq, newGain);
        }
        else // HPF or LPF - frequency only
        {
            bandFreq[draggedBand] = newFreq;
            
            if (onBandDragged)
                onBandDragged(draggedBand, newFreq, 0.0f);
        }
        
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent& event) override
    {
        draggedBand = -1;
    }
    
    void mouseExit(const juce::MouseEvent& event) override
    {
        hoveredBand = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
    
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        int targetBand = draggedBand >= 0 ? draggedBand : hoveredBand;
        
        if (targetBand >= 1 && targetBand <= 4) // Peak bands - adjust Q
        {
            float qDelta = wheel.deltaY * 0.5f;
            bandQ[targetBand] = juce::jlimit(0.3f, 5.0f, bandQ[targetBand] + qDelta);
            
            if (onQChanged)
                onQChanged(targetBand, bandQ[targetBand]);
            
            repaint();
        }
        else if (targetBand == 0 || targetBand == 5) // HPF/LPF - adjust slope
        {
            float slopeDelta = wheel.deltaY * 2.0f; // Slope changes in larger increments
            float newSlope = bandQ[targetBand] * 12.0f; // Convert back to slope (1-8 range for steeper)
            newSlope = juce::jlimit(1.0f, 8.0f, newSlope + slopeDelta);
            bandQ[targetBand] = newSlope / 12.0f;
            
            if (onSlopeChanged)
                onSlopeChanged(targetBand, newSlope);
            
            repaint();
        }
    }
    
    // Set HPF parameters (band 0)
    void setHPF(float freq, float slope)
    {
        bandFreq[0] = freq;
        bandQ[0] = slope / 12.0f; // Convert slope to Q-like value
        bandType[0] = FilterType::HighPass;
        repaint();
    }
    
    // Set parametric band parameters (bands 1-4)
    void setParametricBand(int index, float freq, float gain, float q)
    {
        if (index >= 1 && index <= 4)
        {
            bandFreq[index] = freq;
            bandGain[index] = gain;
            bandQ[index] = q;
            bandType[index] = FilterType::Peak;
            repaint();
        }
    }
    
    // Set LPF parameters (band 5)
    void setLPF(float freq, float slope)
    {
        bandFreq[5] = freq;
        bandQ[5] = slope / 12.0f;
        bandType[5] = FilterType::LowPass;
        repaint();
    }

private:
    enum class FilterType
    {
        HighPass,
        Peak,
        LowPass
    };
    
    FilterType bandType[6] = { FilterType::HighPass, FilterType::Peak, FilterType::Peak, 
                                FilterType::Peak, FilterType::Peak, FilterType::LowPass };
    float bandFreq[6] = { 20.0f, 200.0f, 800.0f, 2000.0f, 6000.0f, 20000.0f }; // HPF left, LPF right
    float bandGain[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float bandQ[6] = { 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f };
    bool bandEnabled[6];
    
    int hoveredBand = -1;
    int draggedBand = -1;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;
    
    float calculateMagnitudeForFrequency(float freq) const
    {
        float magnitude = 0.0f;
        
        for (int i = 0; i < 6; ++i)
        {
            if (!bandEnabled[i] || bandFreq[i] <= 0.0f)
                continue;
                
            float contribution = 0.0f;
            
            switch (bandType[i])
            {
                case FilterType::HighPass:
                {
                    // If slope is 0, filter is bypassed - show flat response
                    if (bandQ[i] <= 0.0f)
                        break;
                        
                    // Only show rolloff for frequencies significantly below cutoff
                    // This keeps the visualization flat when HPF is at minimum (20Hz)
                    float ratio = freq / bandFreq[i];
                    if (ratio < 0.95f) // Below cutoff with margin
                    {
                        float slope = bandQ[i] * 12.0f; // Convert back to actual slope (0-8 range)
                        // Each order contributes 12dB/octave, calculate octaves below cutoff
                        float octaves = std::log2(ratio);
                        // Total attenuation = slope * 12dB/oct * octaves
                        contribution = slope * 12.0f * octaves; // Will be negative
                        contribution = juce::jmax(contribution, -96.0f); // Limit to -96dB for visualization
                    }
                    break;
                }
                    
                case FilterType::Peak:
                {
                    float distance = std::log2(freq / bandFreq[i]);
                    float bandwidth = 1.0f / (bandQ[i] + 0.1f);
                    float response = std::exp(-distance * distance / (2.0f * bandwidth * bandwidth));
                    contribution = bandGain[i] * response;
                    break;
                }
                    
                case FilterType::LowPass:
                {
                    // If slope is 0, filter is bypassed - show flat response
                    if (bandQ[i] <= 0.0f)
                        break;
                        
                    // Only show rolloff for frequencies significantly above cutoff
                    // This keeps the visualization flat when LPF is at maximum (20kHz)
                    float ratio = freq / bandFreq[i];
                    if (ratio > 1.05f) // Above cutoff with margin
                    {
                        float slope = bandQ[i] * 12.0f; // Convert back to actual slope (0-8 range)
                        // Calculate octaves above cutoff
                        float octaves = std::log2(ratio);
                        // Total attenuation = -slope * 12dB/oct * octaves (negative for downward slope)
                        contribution = -slope * 12.0f * octaves;
                        contribution = juce::jmax(contribution, -96.0f); // Limit to -96dB for visualization
                    }
                    break;
                }
            }
            
            magnitude += contribution;
        }
        
        return juce::jlimit(-96.0f, 24.0f, magnitude); // Expanded range for visualization
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQComponent)
};
