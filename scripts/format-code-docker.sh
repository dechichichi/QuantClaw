#!/bin/bash
# QuantClaw Code Formatting Script (Docker-based)
# Formats C++ code using clang-format in a Docker container
# No local installation of clang-format required!

set -e

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${GREEN}QuantClaw Code Formatter (Docker)${NC}"
echo "===================================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo -e "${YELLOW}Error: Docker not found${NC}"
    echo "Please install Docker or use the native script: ./scripts/format-code.sh"
    exit 1
fi

echo "Pulling Docker image (first run only)..."
docker pull silkeh/clang:14 > /dev/null 2>&1 || true

echo "Formatting code..."

# Run clang-format in Docker container
docker run --rm -v "$PROJECT_ROOT:/src" -w /src \
    silkeh/clang:14 \
    bash -c 'find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i -style=file'

echo ""
echo -e "${GREEN}✓ Code formatting complete!${NC}"
echo ""
echo "Files formatted. Review changes with: git diff"
