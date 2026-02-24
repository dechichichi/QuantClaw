# Contributing to QuantClaw

Thank you for your interest in contributing to QuantClaw! This guide will help you set up your development environment and understand our workflow.

## Development Environment Setup

### Prerequisites

- **Compiler**: GCC 11+ or Clang 14+
- **CMake**: 3.20 or later
- **Required Libraries**:
  - spdlog
  - nlohmann-json
  - libcurl
  - OpenSSL

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    ninja-build \
    g++-11 \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    clang-format
```

#### macOS
```bash
brew install cmake ninja gcc spdlog nlohmann-json curl openssl clang-format
```

#### Windows (WSL2)
Follow the Ubuntu/Debian instructions in WSL2.

## Building the Project

```bash
# Clone the repository
git clone https://github.com/yourusername/QuantClaw.git
cd QuantClaw

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
ninja

# Run tests
ctest --output-on-failure
```

## Code Style

QuantClaw follows a coding style based on the Google C++ Style Guide with some modifications. The style is enforced using `clang-format`.

### Formatting Your Code

#### Option 1: Using the provided script
```bash
# Format all code
./scripts/format-code.sh

# Check formatting without modifying files
./scripts/format-code.sh --check
```

#### Option 2: Using clang-format directly
```bash
# Format all C++ files
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

#### Option 3: Using Docker (if clang-format not installed)
```bash
docker run --rm -v "$(pwd):/src" -w /src \
    silkeh/clang:14 \
    bash -c 'find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i -style=file'
```

### IDE Integration

#### VS Code
Install the "C/C++" extension and add to `.vscode/settings.json`:
```json
{
    "C_Cpp.clang_format_style": "file",
    "editor.formatOnSave": true
}
```

#### CLion
CLion automatically detects `.clang-format` files. Enable auto-format on save:
- Settings → Tools → Actions on Save → Reformat code

## Workflow

### 1. Fork and Clone
```bash
git clone https://github.com/YOUR_USERNAME/QuantClaw.git
cd QuantClaw
git remote add upstream https://github.com/ORIGINAL_OWNER/QuantClaw.git
```

### 2. Create a Branch
```bash
git checkout -b feature/my-new-feature
# or
git checkout -b fix/issue-123
```

### 3. Make Changes

- Write code following our style guide
- Add tests for new functionality
- Update documentation as needed
- Format your code: `./scripts/format-code.sh`

### 4. Test Your Changes
```bash
# Build
cmake --build build --parallel

# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/quantclaw_tests --gtest_filter=TestName*
```

### 5. Commit Your Changes
```bash
git add .
git commit -m "feat: add new feature description"
```

**Commit Message Format**:
- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `refactor:` - Code refactoring
- `test:` - Adding tests
- `chore:` - Build/tooling changes

### 6. Push and Create Pull Request
```bash
git push origin feature/my-new-feature
```

Then create a Pull Request on GitHub.

## Pull Request Guidelines

1. **Target Branch**: `main` (or appropriate feature branch)
2. **Code Quality**:
   - All tests must pass
   - Code must be formatted with clang-format
   - No new compiler warnings
3. **Documentation**:
   - Update README if adding features
   - Add inline comments for complex logic
4. **Testing**:
   - Add unit tests for new features
   - Update existing tests if modifying behavior

## Testing

### Running Tests
```bash
# Run all tests
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --output-on-failure --verbose

# Run specific test suite
./build/quantclaw_tests --gtest_filter=AgentLoopTest.*
```

### Writing Tests
Tests use Google Test framework. Example:
```cpp
#include <gtest/gtest.h>
#include "quantclaw/my_module.hpp"

TEST(MyModuleTest, BasicFunctionality) {
    MyModule module;
    EXPECT_TRUE(module.initialize());
    EXPECT_EQ(module.getValue(), 42);
}
```

## Troubleshooting

### Build Failures

**Missing dependencies**:
```bash
# Ubuntu/Debian
sudo apt-get install -y pkg-config

# Check CMake can find dependencies
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

**Compiler errors**:
- Ensure you're using GCC 11+ or Clang 14+
- Check C++17 support: `g++ --version` or `clang++ --version`

### Format Check Failures

If CI reports formatting issues:
```bash
# Install clang-format
sudo apt-get install clang-format

# Format all code
./scripts/format-code.sh

# Commit the formatting changes
git add .
git commit -m "chore: format code with clang-format"
git push
```

## Getting Help

- **Issues**: https://github.com/OWNER/QuantClaw/issues
- **Discussions**: https://github.com/OWNER/QuantClaw/discussions
- **Development Plan**: See `.claude/DEVELOPMENT_PLAN.md`

## License

By contributing to QuantClaw, you agree that your contributions will be licensed under the Apache License 2.0.
