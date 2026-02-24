#!/bin/bash
# QuantClaw Code Formatting Script
# This script formats all C++ source files using clang-format

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}QuantClaw Code Formatter${NC}"
echo "================================"

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format not found${NC}"
    echo ""
    echo "Please install clang-format first:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  macOS:         brew install clang-format"
    echo "  Windows:       Download from LLVM releases"
    exit 1
fi

CLANG_FORMAT_VERSION=$(clang-format --version | grep -oP '\d+\.\d+' | head -1)
echo -e "Using clang-format version: ${GREEN}${CLANG_FORMAT_VERSION}${NC}"
echo ""

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Check if .clang-format exists
if [ ! -f ".clang-format" ]; then
    echo -e "${RED}Error: .clang-format file not found${NC}"
    exit 1
fi

echo "Project root: $PROJECT_ROOT"
echo ""

# Find all C++ source files
echo "Finding C++ source files..."
FILES=$(find src include tests -name "*.cpp" -o -name "*.hpp" 2>/dev/null || true)

if [ -z "$FILES" ]; then
    echo -e "${YELLOW}No C++ files found${NC}"
    exit 0
fi

FILE_COUNT=$(echo "$FILES" | wc -l)
echo -e "Found ${GREEN}${FILE_COUNT}${NC} files to format"
echo ""

# Check if --check flag is passed
if [ "$1" = "--check" ]; then
    echo "Running format check (dry-run)..."
    FAILED=0

    for FILE in $FILES; do
        if ! clang-format --dry-run --Werror "$FILE" 2>&1 > /dev/null; then
            echo -e "${RED}✗${NC} $FILE"
            FAILED=$((FAILED + 1))
        else
            echo -e "${GREEN}✓${NC} $FILE"
        fi
    done

    echo ""
    if [ $FAILED -gt 0 ]; then
        echo -e "${RED}Format check failed: $FAILED file(s) need formatting${NC}"
        echo "Run './scripts/format-code.sh' to fix formatting"
        exit 1
    else
        echo -e "${GREEN}All files are properly formatted!${NC}"
        exit 0
    fi
else
    # Format all files
    echo "Formatting files..."
    FORMATTED=0

    for FILE in $FILES; do
        clang-format -i "$FILE"
        echo -e "${GREEN}✓${NC} $FILE"
        FORMATTED=$((FORMATTED + 1))
    done

    echo ""
    echo -e "${GREEN}Successfully formatted ${FORMATTED} file(s)!${NC}"
fi
