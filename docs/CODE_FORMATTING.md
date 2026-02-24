# Code Formatting Guide

QuantClaw uses `clang-format` to maintain consistent code style across the project.

## Quick Fix

If you see formatting errors in CI, here are the quickest ways to fix them:

### Method 1: Using Docker (Recommended - No installation required)

```bash
cd /path/to/QuantClaw

# Format all code
docker run --rm -v "$(pwd):/src" -w /src \
    silkeh/clang:14 \
    bash -c 'find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i -style=file'
```

### Method 2: Install clang-format locally

**Ubuntu/Debian:**
```bash
sudo apt-get install clang-format
./scripts/format-code.sh
```

**macOS:**
```bash
brew install clang-format
./scripts/format-code.sh
```

**Windows (WSL2):**
```bash
sudo apt-get install clang-format
./scripts/format-code.sh
```

### Method 3: Use our script with Docker

```bash
./scripts/format-code-docker.sh
```

## Verifying Formatting

Before committing, check if your code is properly formatted:

```bash
./scripts/format-code.sh --check
```

## IDE Integration

### VS Code

1. Install the "C/C++" extension
2. Add to `.vscode/settings.json`:
   ```json
   {
       "C_Cpp.clang_format_style": "file",
       "editor.formatOnSave": true
   }
   ```

### CLion

CLion automatically detects `.clang-format` files.

To enable format on save:
1. Go to: **Settings → Tools → Actions on Save**
2. Enable: **Reformat code**

### Vim/Neovim

Add to your `.vimrc` or `init.vim`:
```vim
autocmd FileType cpp,hpp setlocal formatprg=clang-format
```

## What Gets Formatted?

The following files are automatically formatted:
- All `.cpp` files in `src/`, `tests/`
- All `.hpp` files in `include/`, `src/`, `tests/`

## Style Configuration

The `.clang-format` file at the project root defines our code style based on:
- **Base**: Google C++ Style Guide
- **Indent**: 4 spaces (not tabs)
- **Column Limit**: 120 characters
- **Standard**: C++17

See `.clang-format` for full configuration details.

## Common Formatting Issues

### Issue 1: Pointer/Reference Alignment
```cpp
// ❌ Wrong
int* ptr;
const std::string& ref;

// ✅ Correct
int* ptr;
const std::string& ref;
```

### Issue 2: Bracing Style
```cpp
// ❌ Wrong
if (condition)
{
    doSomething();
}

// ✅ Correct
if (condition) {
    doSomething();
}
```

### Issue 3: Line Length
```cpp
// ❌ Wrong - Line too long (>120 chars)
void myVeryLongFunctionNameThatTakesLotsOfParameters(int param1, int param2, int param3, int param4, int param5, int param6) {

// ✅ Correct - Split across lines
void myVeryLongFunctionNameThatTakesLotsOfParameters(int param1, int param2, int param3, int param4,
                                                       int param5, int param6) {
```

## Troubleshooting

### "clang-format: command not found"

Install clang-format or use the Docker method (no installation required).

### Docker permission errors on Linux

Add your user to the docker group:
```bash
sudo usermod -aG docker $USER
newgrp docker
```

### Files not getting formatted

Make sure you're in the project root directory:
```bash
cd /path/to/QuantClaw
./scripts/format-code.sh
```

## Pre-commit Hook (Optional)

To automatically format code before each commit:

```bash
# Create pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
./scripts/format-code.sh
git add -u
EOF

chmod +x .git/hooks/pre-commit
```

## FAQ

**Q: Do I need to format manually every time?**
A: No! Use IDE integration (format on save) or the pre-commit hook to automate it.

**Q: Can I use a different clang-format version?**
A: We recommend clang-format 14, but versions 12-18 should work similarly.

**Q: What if I disagree with the formatting?**
A: Please open a discussion issue. We're open to adjusting the `.clang-format` configuration for valid reasons.

**Q: Does formatting affect generated code?**
A: No, only manually written source files in `src/`, `include/`, and `tests/` are formatted.
