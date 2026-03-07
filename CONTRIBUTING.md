# Contributing to Lisper

Thank you for your interest in contributing to Lisper! This document provides guidelines and instructions for contributing to the project.

## Code of Conduct

- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on constructive criticism
- Respect differing opinions and experiences

## How to Contribute

### Reporting Issues

Before creating an issue:
1. Search existing issues to avoid duplicates
2. Use the issue templates when available
3. Include relevant system information
4. Provide steps to reproduce the problem

### Suggesting Features

1. Check the roadmap and existing issues first
2. Explain the use case and benefits
3. Consider the impact on existing users
4. Be open to discussion and alternatives

### Pull Requests

1. **Fork and Clone**
   ```bash
   git clone https://github.com/yourusername/Lisper.git
   cd Lisper
   git submodule update --init --recursive
   ```

2. **Create a Branch**
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/issue-description
   ```

3. **Make Changes**
   - Follow the coding standards (see below)
   - Add tests for new features
   - Update documentation as needed
   - Keep commits focused and atomic

4. **Test Your Changes**
   ```bash
   cmake -B build
   cmake --build build -j
   ./build/lisper --help  # Basic smoke test
   # Run with a test file if applicable
   ```

5. **Submit PR**
   - Write a clear PR description
   - Reference any related issues
   - Ensure CI passes (when available)
   - Be responsive to feedback

## Coding Standards

### C++ Style Guide

- **C++ Standard**: C++17
- **Indentation**: 2 spaces (no tabs)
- **Line Length**: 80-100 characters preferred
- **Naming Conventions**:
  - Classes: `PascalCase`
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants: `kPascalCase` or `UPPER_SNAKE_CASE`
  - Namespaces: `lowercase`

### Code Organization

```cpp
// File header comment
#include "header.h"  // Own header first

#include <system_headers>  // System headers
#include <algorithm>
#include <vector>

#include "project/headers.h"  // Project headers

namespace project {

// Implementation

}  // namespace project
```

### Best Practices

1. **RAII**: Use RAII for resource management
2. **Smart Pointers**: Prefer `std::unique_ptr` and `std::shared_ptr`
3. **Error Handling**: Use exceptions for exceptional cases, return codes for expected failures
4. **Comments**: Write self-documenting code, comment the "why" not the "what"
5. **Testing**: Add tests for new functionality

### Example Code

```cpp
namespace lisper {

// Processes audio file and returns transcription result
// Throws std::runtime_error if file cannot be processed
TranscriptionResult process_audio(const std::string& file_path) {
  if (!fs::exists(file_path)) {
    throw std::runtime_error("File not found: " + file_path);
  }

  auto audio_data = load_audio(file_path);
  return transcribe(audio_data);
}

}  // namespace lisper
```

## Development Setup

### Required Tools

- CMake 3.14+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- FFmpeg (for audio/video support)
- SDL2 (optional, for GUI)
- clang-format (optional, for formatting)

### Building with Debug Symbols

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### Running Tests

```bash
# If tests are available
cd build
ctest --verbose
```

### Code Formatting

We use clang-format for consistent formatting:

```bash
# Format a file
clang-format -i src/main.cpp

# Format all source files
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## Areas of Contribution

### High Priority

- **Performance Optimization**: Improve transcription speed
- **Model Support**: Add support for new Whisper models
- **Platform Support**: Enhance Windows/Linux compatibility
- **GUI Features**: Improve the desktop application

### Good First Issues

- Documentation improvements
- Code cleanup and refactoring
- Adding unit tests
- Fixing compiler warnings
- Improving error messages

### Feature Ideas

- Cloud model download/management
- Plugin system for output formats
- Batch processing improvements
- Real-time streaming support
- Mobile app development

## Project Structure

```
Lisper/
├── src/              # Main source code
│   ├── gui/         # GUI components
│   └── *.cpp/h      # Core functionality
├── docs/            # Documentation
├── tests/           # Test files
├── models/          # Model storage (gitignored)
└── third_party/     # Dependencies
```

## Documentation

When adding new features:
1. Update the README.md with usage examples
2. Add inline documentation for public APIs
3. Update relevant docs/ files
4. Include examples if applicable

## Release Process

1. Version numbers follow semantic versioning (MAJOR.MINOR.PATCH)
2. Update CHANGELOG.md with notable changes
3. Tag releases with `v` prefix (e.g., `v1.2.3`)
4. Include pre-built binaries when possible

## Getting Help

- **Discord**: [Join our community](https://discord.gg/lisper) (if available)
- **Issues**: Use GitHub issues for bugs and features
- **Discussions**: GitHub Discussions for general questions

## Recognition

Contributors will be:
- Listed in the project README
- Credited in release notes
- Given collaborator access for sustained contributions

## License

By contributing to Lisper, you agree that your contributions will be licensed under the MIT License.

Thank you for helping make Lisper better!