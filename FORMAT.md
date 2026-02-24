# Quick Format Fix

If CI reports formatting errors, run ONE of these commands:

## Option 1: Docker (No installation needed)
```bash
docker run --rm -v "$(pwd):/src" -w /src silkeh/clang:14 \
    bash -c 'find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i -style=file'
```

## Option 2: Install clang-format locally
```bash
# Ubuntu/Debian
sudo apt-get install clang-format
./scripts/format-code.sh

# macOS
brew install clang-format
./scripts/format-code.sh
```

## Option 3: Use our Docker script
```bash
./scripts/format-code-docker.sh
```

---

After formatting, commit the changes:
```bash
git add .
git commit -m "chore: format code with clang-format"
git push
```

For more details, see [docs/CODE_FORMATTING.md](docs/CODE_FORMATTING.md)
