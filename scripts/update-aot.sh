#!/bin/sh
# Compile zeroperl.wasm to AOT and install into libexif resources.
#
# Usage:
#   ./scripts/update-aot.sh <path/to/zeroperl.wasm>
#   ZEROPERL_WASM=path/to/zeroperl.wasm ./scripts/update-aot.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WAMRC_BUILD="$PROJECT_DIR/build/wamrc"
WAMRC="$WAMRC_BUILD/wamrc"
WASM_INPUT="${1:-${ZEROPERL_WASM:-}}"
AOT_OUTPUT="$PROJECT_DIR/resources/zeroperl.aot"

if [ -z "$WASM_INPUT" ]; then
    echo "Usage: $0 <path/to/zeroperl.wasm>" >&2
    echo "  or: ZEROPERL_WASM=<path> $0" >&2
    exit 1
fi

if [ ! -f "$WASM_INPUT" ]; then
    echo "error: $WASM_INPUT not found" >&2
    exit 1
fi

# Build wamrc if needed
if [ ! -x "$WAMRC" ]; then
    echo "Building wamrc..."
    LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"
    cmake -B "$WAMRC_BUILD" -S "$PROJECT_DIR/vendor/wamr/wamr-compiler" \
        -DCMAKE_BUILD_TYPE=Release \
        -DWAMR_BUILD_WITH_CUSTOM_LLVM=1 \
        -DLLVM_DIR="$LLVM_DIR" 2>&1
    cmake --build "$WAMRC_BUILD" --parallel 2>&1
fi

# Strip empty name section that wamrc cannot parse
python3 -c "
import sys
data = open(sys.argv[1], 'rb').read()
marker = b'\x00\x05\x04name'
idx = data.rfind(marker)
if idx > 0 and idx + len(marker) == len(data):
    open(sys.argv[1], 'wb').write(data[:idx])
    print('Stripped empty name section')
" "$WASM_INPUT"

echo "Compiling $(basename "$WASM_INPUT") -> zeroperl.aot"
"$WAMRC" -o "$AOT_OUTPUT" "$WASM_INPUT"

echo "Installed: $AOT_OUTPUT ($(wc -c < "$AOT_OUTPUT" | tr -d ' ') bytes)"
