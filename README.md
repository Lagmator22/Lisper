# Lisper - Local Speech-to-Text Engine

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey.svg)](https://github.com/Lagmator22/Lisper)

Local speech-to-text engine built in C++. Transcribes audio and video files using [whisper.cpp](https://github.com/ggerganov/whisper.cpp).

**Privacy-first design** - All transcription happens locally on your hardware. No cloud APIs, no data leaves your machine.

</div>

## Key Features

- **Multi-format Support**: Transcribe audio (WAV, MP3, FLAC, OGG, M4A, AAC, WMA, AIFF) and video files (MP4, MKV, AVI, WEBM, MOV)
- **Multiple Output Formats**: Plain text, SRT subtitles, JSON with timestamps, RAG-ready chunks
- **Batch Processing**: Transcribe entire directories in one command
- **Watch Mode**: Monitor folders and auto-transcribe new files as they appear
- **Live Transcription**: Real-time microphone transcription (when compiled with SDL2)
- **Model Profiles**: Pre-configured settings for fast/balanced/quality/max transcription
- **Cross-platform**: Works on macOS, Windows, and Linux
- **Hardware Acceleration**: Broad hardware support via whisper.cpp backends
- **Desktop GUI**: Native application with drag-and-drop support
- **Beautiful CLI**: Animated interface with gradient styling and progress indicators

## Quick Start

### Prerequisites

- CMake 3.14+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- FFmpeg (for audio/video processing)
- SDL2 (optional, for GUI and live transcription)

#### Install dependencies

**macOS:**
```bash
brew install cmake ffmpeg sdl2
```

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install cmake build-essential ffmpeg libsdl2-dev
```

**Windows:**
- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ workload
- Install [CMake](https://cmake.org/download/)
- Install [FFmpeg](https://ffmpeg.org/download.html) and add to PATH
- Install [SDL2](https://www.libsdl.org/download-2.0.php) (optional)

### Build from Source

```bash
# Clone the repository
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

# Build the project
cmake -B build
cmake --build build -j

# Download a model (required for transcription)
cd models
# Choose one:
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin     # Fast (190MB)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin  # Quality (574MB)
cd ..

# Test the installation
./build/lisper --help
```

## Usage Examples

### CLI - Simple Transcription

```bash
# Transcribe using auto-selected model profile
./build/lisper --model-profile quality recording.wav

# Transcribe with specific model
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin audio.mp3

# Generate SRT subtitles
./build/lisper --model-profile quality video.mp4 -f srt -o video.srt

# JSON output with timestamps
./build/lisper -m models/ggml-small-q5_1.bin podcast.mp3 -f json -o transcript.json
```

### CLI - Advanced Features

```bash
# Batch transcribe all media in a directory
./build/lisper --model-profile quality -d ./recordings/ -o ./transcripts/

# Watch mode - auto-transcribe new files as they appear
./build/lisper --model-profile quality -w ./inbox/ -o ./transcripts/ -f srt

# Live microphone transcription (requires SDL2)
./build/lisper --model-profile fast --live

# Translate to English
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin spanish_audio.mp3 --translate

# Force CPU mode (useful for testing)
./build/lisper --model-profile quality --device cpu recording.wav

# RAG-ready output (chunked for vector databases)
./build/lisper --model-profile quality document.mp4 -f rag -o ./knowledge_base/
```

### GUI Application

```bash
# Launch the desktop application
./build/lisper_gui
```

The GUI provides:
- Drag-and-drop file selection
- Visual waveform display
- Real-time transcription progress
- Model selection interface
- Export options for all formats
- Batch processing support
- Settings persistence

## Model Profiles

Pre-configured profiles for different use cases:

| Profile | Model | Size | Use Case |
|---------|-------|------|----------|
| `fast` | small-q5_1 | 190MB | Quick drafts, real-time transcription |
| `balanced` | medium-q5_0 | 514MB | General use, good accuracy/speed balance |
| `quality` | large-v3-turbo-q5_0 | 574MB | Best quality/speed tradeoff |
| `max` | large-v3 | 1.5GB | Maximum accuracy, when latency doesn't matter |

Use `./build/lisper --list-model-profiles` to see all available profiles.

## Output Formats

| Format | Flag | Description | Example Use |
|--------|------|-------------|-------------|
| Text | `-f text` | Plain transcription text (default) | Documentation, notes |
| SRT | `-f srt` | SubRip subtitle format with timestamps | Video subtitles |
| JSON | `-f json` | Structured output with word-level timestamps | Analysis, processing |
| RAG | `-f rag` | Chunked text optimized for vector databases | AI knowledge bases |

## Live Transcription Mode

When compiled with SDL2 support, Lisper can transcribe from your microphone in real-time:

```bash
# Start live transcription
./build/lisper --model-profile fast --live

# Controls:
# - Press SPACE to pause/resume
# - Press ENTER to save current transcript
# - Press ESC to exit
```

Live mode features:
- Real-time speech detection
- Automatic silence detection
- Rolling buffer for continuous transcription
- Low-latency processing
- Adjustable sensitivity

## Performance Optimization

### Performance Tips

1. **Model Selection**: Choose the smallest model that meets your accuracy needs
2. **Thread Count**: Use `-t` to adjust threads (default: 4)
3. **Device Selection**: Use `--device gpu` for supported hardware
4. **Flash Attention**: Enabled by default, disable with `--no-flash-attn` if issues occur
5. **Batch Processing**: Process multiple files together for better efficiency

## Architecture

```
Lisper/
├── src/
│   ├── main.cpp              # CLI entry point
│   ├── gui_main.cpp          # GUI entry point
│   ├── lisper.cpp            # Core transcription engine
│   ├── media.cpp             # Audio/video processing
│   ├── formatter.cpp         # Output formatting
│   ├── live.cpp              # Real-time transcription
│   ├── watcher.cpp           # Directory monitoring
│   └── gui/                  # GUI components
│       ├── theme.cpp         # Visual styling
│       ├── layout.cpp        # UI layout
│       └── state.cpp         # Application state
├── models/                   # Model storage (gitignored)
├── docs/                     # Documentation
└── third_party/
    └── whisper.cpp/          # Whisper engine (submodule)
```

## Troubleshooting

### Common Issues

**"Model not found" error:**
- Ensure you've downloaded a model to the `models/` directory
- Check the model filename matches the profile or path specified

**"SDL_Init Error" when using --live:**
- Install SDL2 development libraries
- Rebuild with SDL2 support: `cmake -B build -DLISPER_ENABLE_LIVE=ON`

**Poor transcription quality:**
- Try a larger model (quality or max profile)
- Ensure audio is clear and properly formatted
- Check language setting matches audio language

**FFmpeg errors:**
- Verify FFmpeg is installed and in PATH
- Check input file format is supported
- Try converting to WAV format first

### Debug Mode

Enable verbose output for troubleshooting:
```bash
# Set environment variable
export WHISPER_DEBUG=1
./build/lisper --model-profile quality audio.wav
```

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

Areas of interest:
- Additional output formats
- Performance optimizations
- Language-specific improvements
- GUI enhancements
- Platform-specific features

## Building Documentation

Detailed platform-specific build instructions are available in [BUILDING.md](BUILDING.md).

For API documentation (using Lisper as a library), see [docs/API.md](docs/API.md).

Model selection guide: [docs/MODELS.md](docs/MODELS.md).

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Credits

- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) - Core transcription engine
- [OpenAI Whisper](https://github.com/openai/whisper) - Original model architecture
- [Dear ImGui](https://github.com/ocornut/imgui) - GUI framework
- [SDL2](https://www.libsdl.org/) - Cross-platform media layer

## Contact & Support

- **Issues**: [GitHub Issues](https://github.com/Lagmator22/Lisper/issues)
- **Discussions**: [GitHub Discussions](https://github.com/Lagmator22/Lisper/discussions)

---

<div align="center">
Built with dedication to privacy and local-first computing.
</div>