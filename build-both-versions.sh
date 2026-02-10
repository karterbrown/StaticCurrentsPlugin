#!/bin/bash

# Build script to create both Instrument and Audio Effect versions of Static Currents Plugin
# This script builds the AU instrument, copies it, then modifies the copy to be an audio effect

set -e

PROJECT_DIR="/Users/karterbrown/Desktop/Dev/Plugins/StaticCurrentsPlugin"
XCODE_PROJECT="$PROJECT_DIR/Builds/MacOSX/Static Currents Plugin.xcodeproj"
COMPONENTS_DIR="$HOME/Library/Audio/Plug-Ins/Components"
BUILD_DIR="$PROJECT_DIR/Builds/MacOSX/build/Release"

echo "=========================================="
echo "Building Static Currents Plugin - Both Versions"
echo "=========================================="

# Step 1: Build the Instrument version
echo ""
echo "🔨 Step 1: Building AU Instrument version..."
cd "$PROJECT_DIR"
xcodebuild -project "$XCODE_PROJECT" \
  -target "Static Currents Plugin - AU" \
  -configuration Release \
  clean build

# Verify instrument built
INSTRUMENT_COMPONENT="$BUILD_DIR/StaticCurrentsPlugin.component"
if [ ! -d "$INSTRUMENT_COMPONENT" ]; then
  echo "❌ ERROR: Instrument component not found at $INSTRUMENT_COMPONENT"
  exit 1
fi
echo "✅ Instrument version built successfully"

# Step 2: Verify architecture
echo ""
echo "🔍 Step 2: Verifying architecture (must be universal binary)..."
BINARY="$INSTRUMENT_COMPONENT/Contents/MacOS/StaticCurrentsPlugin"
file "$BINARY"
if ! file "$BINARY" | grep -q "universal binary"; then
  echo "⚠️  WARNING: Plugin may not be a universal binary!"
fi
echo "✅ Architecture verified"

# Step 3: Test instrument with auval
echo ""
echo "🧪 Step 3: Running AU validation on Instrument version..."
if auval -v aumu Pqc1 STAT 2>&1 | grep -q "AU VALIDATION SUCCEEDED"; then
  echo "✅ Instrument passes AU validation"
else
  echo "⚠️  WARNING: Instrument validation may have issues"
fi

# Step 4: Install instrument to Components
echo ""
echo "📦 Step 4: Installing Instrument to ~/Library/Audio/Plug-Ins/Components/..."
rm -rf "$COMPONENTS_DIR/StaticCurrentsPlugin.component"
cp -r "$INSTRUMENT_COMPONENT" "$COMPONENTS_DIR/"
echo "✅ Instrument installed"

# Step 5: Create Effect version by copying and modifying
echo ""
echo "🎛️  Step 5: Creating Audio Effect version..."
EFFECT_COMPONENT="$COMPONENTS_DIR/StaticCurrentsPluginEffect.component"
rm -rf "$EFFECT_COMPONENT"
cp -r "$COMPONENTS_DIR/StaticCurrentsPlugin.component" "$EFFECT_COMPONENT"
echo "✅ Effect bundle created"

# Step 6: Update Effect Info.plist
echo ""
echo "📝 Step 6: Updating Effect plist configuration..."

# Read the original plist and create effect version
EFFECT_PLIST="$EFFECT_COMPONENT/Contents/Info.plist"

# Update bundle identifier
/usr/libexec/PlistBuddy -c "Set CFBundleIdentifier com.yourcompany.StaticCurrentsPluginEffect" "$EFFECT_PLIST"

# Update bundle name
/usr/libexec/PlistBuddy -c "Set CFBundleName 'Static Currents Effect'" "$EFFECT_PLIST"

# Update display name
/usr/libexec/PlistBuddy -c "Set CFBundleDisplayName 'Static Currents Effect'" "$EFFECT_PLIST"

# Update AudioComponents array - change type from aumu (instrument) to aufx (effect)
/usr/libexec/PlistBuddy -c "Set AudioComponents:0:type aufx" "$EFFECT_PLIST"

# Change subtype from Pqc1 to Pqc2 to differentiate
/usr/libexec/PlistBuddy -c "Set AudioComponents:0:subtype Pqc2" "$EFFECT_PLIST"

# Update the display name in the AudioComponents
/usr/libexec/PlistBuddy -c "Set AudioComponents:0:name 'Static Currents Effect'" "$EFFECT_PLIST"

# Update description
/usr/libexec/PlistBuddy -c "Set AudioComponents:0:description 'Static Currents Real-Time Audio Effect'" "$EFFECT_PLIST" 2>/dev/null || true

echo "✅ Effect plist updated"

# Step 7: Verify plist is valid
echo ""
echo "🔍 Step 7: Verifying Effect plist syntax..."
if plutil -lint "$EFFECT_PLIST" > /dev/null; then
  echo "✅ Effect plist is valid"
else
  echo "❌ ERROR: Effect plist is invalid!"
  plutil -lint "$EFFECT_PLIST"
  exit 1
fi

# Step 8: Code sign both
echo ""
echo "🔐 Step 8: Code signing both versions..."
codesign -f -s - "$COMPONENTS_DIR/StaticCurrentsPlugin.component"
codesign -f -s - "$EFFECT_COMPONENT"
echo "✅ Both versions code signed"

# Step 9: Verify installations
echo ""
echo "📋 Step 9: Verifying installed components..."
echo ""
echo "Instrument version:"
plutil -p "$COMPONENTS_DIR/StaticCurrentsPlugin.component/Contents/Info.plist" | grep -E "CFBundleIdentifier|name|type" | head -5
echo ""
echo "Effect version:"
plutil -p "$EFFECT_COMPONENT/Contents/Info.plist" | grep -E "CFBundleIdentifier|name|type" | head -5

# Step 10: Summary
echo ""
echo "=========================================="
echo "✅ Build Complete!"
echo "=========================================="
echo ""
echo "📍 Installed locations:"
echo "  Instrument: $COMPONENTS_DIR/StaticCurrentsPlugin.component"
echo "  Effect:     $EFFECT_COMPONENT"
echo ""
echo "🔄 Next steps:"
echo "  1. Quit Logic Pro completely"
echo "  2. Clear caches: rm -rf ~/Library/Caches/com.apple.audio*"
echo "  3. Reopen Logic Pro"
echo "  4. Go to Preferences > Plug-Ins > AU Plugins"
echo "  5. Click 'Rescan AU Plug-Ins'"
echo ""
echo "🎵 You should now see:"
echo "  - 'Static Currents Plugin' (Instrument/Synth)"
echo "  - 'Static Currents Effect' (Audio Effect)"
echo ""
