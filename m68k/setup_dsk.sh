#!/bin/bash
set -e

# Build paths
BUILD_DIR="./build"
IMG="$BUILD_DIR/banks.dsk"

# Create fresh 800K image
dd if=/dev/zero of="$IMG" bs=1024 count=800
hformat -l FLIGHT "$IMG"

# Mount and copy files
hmount "$IMG"
hcopy "$BUILD_DIR/banks" ::
hcopy "pittsburgh.scene" ::
hcopy "horizon.scene" ::

# Set type to APPL if hfsutils-extra is installed
if command -v hsettype >/dev/null 2>&1; then
  hsettype banks APPL
  hsetcreator banks '????'
else
  echo "⚠️  hsettype not found — skipping type assignment"
  echo "✅ You can still run this in Mini vMac and assign APPL via ResEdit if needed."
fi

humount

echo "✅ Disk image created: $IMG"
