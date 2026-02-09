#!/usr/bin/env python3
"""Script to generate new 6-band parametric EQ slider initialization code"""

# Generate initialization code for all 16 new EQ sliders
eq_sliders = [
    ("hpfFreq", "HPF Freq", "20.0, 500.0, 1.0", "80.0", " Hz"),
    ("hpfSlope", "HPF Slope", "1.0, 4.0, 1.0", "1.0", ""),
    
    ("peak1Freq", "Peak1 Freq", "20.0, 500.0, 1.0", "200.0", " Hz"),
    ("peak1Gain", "Peak1 Gain", "-12.0, 12.0, 0.1", "0.0", " dB"),
    ("peak1Q", "Peak1 Q", "0.1, 10.0, 0.1", "0.7", ""),
    
    ("peak2Freq", "Peak2 Freq", "100.0, 2000.0, 1.0", "800.0", " Hz"),
    ("peak2Gain", "Peak2 Gain", "-12.0, 12.0, 0.1", "0.0", " dB"),
    ("peak2Q", "Peak2 Q", "0.1, 10.0, 0.1", "0.7", ""),
    
    ("peak3Freq", "Peak3 Freq", "500.0, 5000.0, 1.0", "2000.0", " Hz"),
    ("peak3Gain", "Peak3 Gain", "-12.0, 12.0, 0.1", "0.0", " dB"),
    ("peak3Q", "Peak3 Q", "0.1, 10.0, 0.1", "0.7", ""),
    
    ("peak4Freq", "Peak4 Freq", "1000.0, 10000.0, 1.0", "6000.0", " Hz"),
    ("peak4Gain", "Peak4 Gain", "-12.0, 12.0, 0.1", "0.0", " dB"),
    ("peak4Q", "Peak4 Q", "0.1, 10.0, 0.1", "0.7", ""),
    
    ("lpfFreq", "LPF Freq", "3000.0, 20000.0, 1.0", "12000.0", " Hz"),
    ("lpfSlope", "LPF Slope", "1.0, 4.0, 1.0", "1.0", ""),
]

print("// 6-Band Parametric EQ Sliders")
print("addAndMakeVisible(eqVisualization);")
print()

for slider_name, label, range_vals, default, suffix in eq_sliders:
    var_name = f"{slider_name}Slider"
    method_prefix = slider_name[0].upper() + slider_name[1:]
    
    print(f"addAndMakeVisible({var_name});")
    print(f"{var_name}.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);")
    print(f"{var_name}.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);")
    print(f"{var_name}.setRange({range_vals});")
    print(f"{var_name}.setValue({default});")
    if suffix:
        print(f"{var_name}.setTextValueSuffix(\"{suffix}\");")
    print(f"{var_name}.onValueChange = [this] {{ *audioProcessor.get{method_prefix}Parameter() = static_cast<float>({var_name}.getValue()); updateEQVisualization(); }};")
    print()
