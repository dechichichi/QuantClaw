#!/bin/bash
set -e

UI_DIR="${HOME}/.quantclaw/ui"
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

echo "Cloning OpenClaw UI..."
git clone --depth 1 https://github.com/openclaw/openclaw.git "$TEMP_DIR/openclaw"

cd "$TEMP_DIR/openclaw/ui"

echo "Installing dependencies..."
if command -v pnpm &> /dev/null; then
    pnpm install
elif command -v npm &> /dev/null; then
    npm install
else
    echo "Error: npm or pnpm is required"
    exit 1
fi

echo "Building..."
if command -v pnpm &> /dev/null; then
    pnpm build
else
    npm run build
fi

echo "Installing to $UI_DIR..."
rm -rf "$UI_DIR"
mkdir -p "$UI_DIR"
cp -r dist/* "$UI_DIR/"

# Inject gateway config into index.html
sed -i 's|<head>|<head><script>window.__QUANTCLAW_GATEWAY_WS_PORT=18800;</script>|' "$UI_DIR/index.html"

echo "Done. Dashboard UI installed at $UI_DIR"
