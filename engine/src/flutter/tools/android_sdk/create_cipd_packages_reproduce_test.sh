#!/bin/bash
# Copyright 2013 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A reproduction test for flutter/flutter#138021.
# This test sets up a mock environment to verify the dry-run option of create_cipd_packages.sh.

set -e

# Establish paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
TARGET_SCRIPT="$SCRIPT_DIR/create_cipd_packages.sh"

# Create a temporary directory for our mock environment
TEST_TEMP_DIR=$(mktemp -d -t android_sdk_test_XXXXXX)
echo "Created test temp directory: $TEST_TEMP_DIR"

# Clean up on exit
cleanup() {
  echo "Cleaning up $TEST_TEMP_DIR..."
  rm -rf "$TEST_TEMP_DIR"
}
trap cleanup EXIT

# 1. Create a mock bin directory for PATH overriding
MOCK_BIN="$TEST_TEMP_DIR/bin"
mkdir -p "$MOCK_BIN"

# 2. Create mock 'cipd' command (uses escaped \$@ and expanded $TEST_TEMP_DIR)
MOCK_CIPD="$MOCK_BIN/cipd"
cat << EOF > "$MOCK_CIPD"
#!/bin/bash
echo "MOCK CIPD CALLED: \$@" >> "$TEST_TEMP_DIR/cipd_calls.log"
EOF
chmod +x "$MOCK_CIPD"

# Make sure our mock bin is first in PATH so the script uses our mock cipd
export PATH="$MOCK_BIN:$PATH"

# Verify 'which' finds our mock cipd
if [[ "$(which cipd)" != "$MOCK_CIPD" ]]; then
  echo "Error: PATH overriding failed. 'which cipd' returned $(which cipd), expected $MOCK_CIPD."
  exit 1
fi

# 3. Create a mock Android SDK structure
MOCK_SDK="$TEST_TEMP_DIR/mock_sdk"
MOCK_SDK_BIN="$MOCK_SDK/cmdline-tools/latest/bin"
mkdir -p "$MOCK_SDK_BIN"

# Create mock 'sdkmanager' command inside the mock SDK (dynamic directory creation)
MOCK_SDKMANAGER="$MOCK_SDK_BIN/sdkmanager"
cat << 'EOF' > "$MOCK_SDKMANAGER"
#!/bin/bash
# Mock sdkmanager that simulates successful downloads by creating expected directories.
sdk_root=""
packages=()
for arg in "$@"; do
  if [[ $arg == --sdk_root=* ]]; then
    sdk_root="${arg#*=}"
  elif [[ $arg != -* ]]; then
    packages+=("$arg")
  fi
done

if [[ -n "$sdk_root" ]]; then
  mkdir -p "$sdk_root"
  # Always create licenses directory as it is always copied
  mkdir -p "$sdk_root/licenses"
  
  for pkg in "${packages[@]}"; do
    # Extract the top-level directory name (before ';')
    dir_name="${pkg%%;*}"
    if [[ -n "$dir_name" ]]; then
      mkdir -p "$sdk_root/$dir_name"
    fi
  done
fi
EOF
chmod +x "$MOCK_SDKMANAGER"

# 4. Run the script with dry-run option
echo "Running create_cipd_packages.sh with dry-run..."

# We run the script from its containing directory because it expects packages.txt to be there.
cd "$SCRIPT_DIR"

# Before the fix, this command is expected to fail because:
#   - If --dry-run is passed as the first argument, it fails the version tag regex check.
#   - If it's passed as a flag, the script doesn't recognize it and fails or attempts to upload.
# Let's run with --dry-run as a flag. We'll design the flag as --dry-run.
set +e
"$TARGET_SCRIPT" --dry-run "testtag" "$MOCK_SDK" > "$TEST_TEMP_DIR/output.log" 2>&1
EXIT_CODE=$?
set -e

cat "$TEST_TEMP_DIR/output.log"

# Check if the dry run succeeded and did NOT call cipd
if [[ $EXIT_CODE -ne 0 ]]; then
  echo "Test Failed: Script exited with non-zero code $EXIT_CODE."
  exit 1
fi

if [[ -f "$TEST_TEMP_DIR/cipd_calls.log" ]]; then
  echo "Test Failed: cipd was called during a dry-run! Calls:"
  cat "$TEST_TEMP_DIR/cipd_calls.log"
  exit 1
fi

# Extract the temp directory path from the output log
TEMP_DIR_LINE=$(grep "All bundles prepared in:" "$TEST_TEMP_DIR/output.log")
if [[ -z "$TEMP_DIR_LINE" ]]; then
  echo "Test Failed: Could not find temp directory in output log."
  exit 1
fi
PREPARED_TEMP_DIR=${TEMP_DIR_LINE#*All bundles prepared in: }

# Verify that the prepared temp directory exists
if [[ ! -d "$PREPARED_TEMP_DIR" ]]; then
  echo "Test Failed: Prepared temp directory $PREPARED_TEMP_DIR does not exist!"
  exit 1
fi

# Verify that upload directories exist and contain expected files,
# and that sdk directories were cleaned up.
for platform in "linux" "macosx" "windows"; do
  if [[ ! -d "$PREPARED_TEMP_DIR/upload_$platform" ]]; then
    echo "Test Failed: upload_$platform directory does not exist!"
    exit 1
  fi
  if [[ ! -d "$PREPARED_TEMP_DIR/upload_$platform/sdk/licenses" ]]; then
    echo "Test Failed: upload_$platform/sdk/licenses does not exist!"
    exit 1
  fi
  if [[ -d "$PREPARED_TEMP_DIR/sdk_$platform" ]]; then
    echo "Test Failed: sdk_$platform directory was not cleaned up!"
    exit 1
  fi
done

# Clean up the script's temp dir since it was retained in dry-run
rm -rf "$PREPARED_TEMP_DIR"

echo "Test Passed: Dry run completed successfully without calling cipd and verified bundle integrity!"
exit 0
