#!/bin/bash
# QuantClaw Code Formatting Script (Docker-based)
# Formats C++ code using clang-format-18 in a Docker container.
# No local installation of clang-format required.

set -euo pipefail

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARGS=("$@")

echo -e "${GREEN}QuantClaw Code Formatter (Docker)${NC}"
echo "===================================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check if Docker is available
if ! command -v docker >/dev/null 2>&1; then
    echo -e "${YELLOW}Error: Docker not found${NC}"
    echo "Please install Docker or use the native script: ./scripts/format-code.sh"
    exit 1
fi

echo "Starting Docker formatter with clang-format-18..."

# Run the same repository script inside a pinned Ubuntu 24.04 container.
docker run --rm -v "$PROJECT_ROOT:/src" -w /src \
    ubuntu:24.04 \
    bash -lc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update > /dev/null
        apt-get install -y --no-install-recommends clang-format-18 > /dev/null
        CLANG_FORMAT_BIN=clang-format-18 ./scripts/format-code.sh "$@"
    ' bash "${ARGS[@]}"

echo ""
echo -e "${GREEN}OK${NC} Code formatting complete!"
echo ""
echo "Files formatted. Review changes with: git diff"
