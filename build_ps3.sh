#!/bin/bash
set -e

# PSL1GHT Environment Configuration
# Standard paths for PS3DEV/PSL1GHT toolchain
export PS3DEV=${PS3DEV:-/usr/local/ps3dev}
export PSL1GHT=${PSL1GHT:-$PS3DEV/ppu}
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin

# PS3 Game Metadata
GAME_ID="SA2PS3001"
GAME_TITLE="Sonic Advance 2 Decomp"

echo "------------------------------------------"
echo "Building Sonic Advance 2 for PlayStation 3"
echo "Game ID: $GAME_ID"
echo "------------------------------------------"

# 1. Clean and Compile
# The Makefile already uses 'fself' for the PS3 platform to generate the .self binary
echo "[1/4] Cleaning and compiling source code..."
make PLATFORM=ps3 clean
make PLATFORM=ps3 -j$(nproc)

# 2. Generate Metadata (PARAM.SFO and ICON0.PNG)
echo "[2/4] Generating metadata and XMB assets..."
mkdir -p build
python3 scripts/make_ps3_sfo.py "$GAME_ID" "$GAME_TITLE" build/PARAM.SFO
python3 scripts/prepare_ps3_icon.py .github/media/titlescreen.png build/ICON0.PNG

# 3. Organize Package Structure
echo "[3/4] Organizing files into USRDIR structure..."
python3 scripts/ps3_package.py . sa2

# 4. Finalize PKG
# Uses the requested package_finalize tool to create the installable .pkg
echo "[4/4] Finalizing the .pkg file..."
if command -v package_finalize >/dev/null 2>&1; then
    # Finalize the package directory
    package_finalize build/ps3/pkg

    # Move the generated pkg to the root if it was created inside the build dir
    find build/ps3/pkg -name "*.pkg" -exec mv {} ./"$GAME_ID.pkg" \;

    echo "Success! $GAME_ID.pkg has been created."
else
    echo "Error: 'package_finalize' tool not found in PATH."
    echo "The package structure is ready at 'build/ps3/pkg' for manual finalization."
fi
