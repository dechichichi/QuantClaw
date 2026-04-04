#!/bin/bash
# QuantClaw Code Formatting Script
# This script formats all C++ source files using clang-format-18

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}QuantClaw Code Formatter${NC}"
echo "================================"

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

extract_major_version() {
    sed -E 's/.*version ([0-9]+).*/\1/;t;d'
}

detect_clang_format() {
    local candidate=""
    local version_output=""
    local major_version=""
    local brew_prefix=""

    if [ -n "${CLANG_FORMAT_BIN:-}" ]; then
        if [ ! -x "${CLANG_FORMAT_BIN}" ]; then
            echo -e "${RED}Error: CLANG_FORMAT_BIN is set but not executable: ${CLANG_FORMAT_BIN}${NC}"
            exit 1
        fi
        echo "${CLANG_FORMAT_BIN}"
        return 0
    fi

    if command -v clang-format-18 >/dev/null 2>&1; then
        command -v clang-format-18
        return 0
    fi

    if command -v clang-format >/dev/null 2>&1; then
        candidate="$(command -v clang-format)"
        version_output="$("${candidate}" --version 2>/dev/null || true)"
        major_version="$(printf '%s\n' "${version_output}" | extract_major_version)"
        if [ "${major_version}" = "18" ]; then
            echo "${candidate}"
            return 0
        fi
    fi

    if command -v brew >/dev/null 2>&1; then
        brew_prefix="$(brew --prefix llvm@18 2>/dev/null || true)"
        if [ -n "${brew_prefix}" ] && [ -x "${brew_prefix}/bin/clang-format" ]; then
            echo "${brew_prefix}/bin/clang-format"
            return 0
        fi
    fi

    echo -e "${RED}Error: clang-format-18 not found${NC}"
    echo ""
    echo "Install clang-format-18 (the version used by CI), or set CLANG_FORMAT_BIN."
    echo "  Ubuntu/Debian: sudo apt-get install clang-format-18"
    echo "  macOS:         brew install llvm@18"
    echo "                 export CLANG_FORMAT_BIN=\"\$(brew --prefix llvm@18)/bin/clang-format\""
    echo "  Windows:       Install LLVM 18 and set CLANG_FORMAT_BIN to clang-format.exe"
    exit 1
}

CLANG_FORMAT_BIN="$(detect_clang_format)"
CLANG_FORMAT_VERSION="$("${CLANG_FORMAT_BIN}" --version)"
echo -e "Using clang-format binary: ${GREEN}${CLANG_FORMAT_BIN}${NC}"
echo -e "Using clang-format version: ${GREEN}${CLANG_FORMAT_VERSION}${NC}"
echo ""

# Find all C++ source files
echo "Finding C++ source files..."
FILES=()
while IFS= read -r -d '' FILE; do
    FILES+=("$FILE")
done < <(find src include tests \( -name "*.cpp" -o -name "*.hpp" \) -print0 2>/dev/null || true)

if [ "${#FILES[@]}" -eq 0 ]; then
    echo -e "${YELLOW}No C++ files found${NC}"
    exit 0
fi

FILE_COUNT="${#FILES[@]}"
echo -e "Found ${GREEN}${FILE_COUNT}${NC} files to format"
echo ""

# Check if --check flag is passed
if [ "${1:-}" = "--check" ]; then
    echo "Running format check (dry-run)..."
    FAILED=0

    for FILE in "${FILES[@]}"; do
        if ! "${CLANG_FORMAT_BIN}" --dry-run --Werror "$FILE" > /dev/null 2>&1; then
            echo -e "${RED}FAIL${NC} $FILE"
            FAILED=$((FAILED + 1))
        else
            echo -e "${GREEN}OK${NC}   $FILE"
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

    for FILE in "${FILES[@]}"; do
        "${CLANG_FORMAT_BIN}" -i "$FILE"
        echo -e "${GREEN}OK${NC}   $FILE"
        FORMATTED=$((FORMATTED + 1))
    done

    echo ""
    echo -e "${GREEN}Successfully formatted ${FORMATTED} file(s)!${NC}"
fi
