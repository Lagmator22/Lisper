# Lisper

Local speech-to-text engine built in C++. Transcribes audio and video files using [whisper.cpp](https://github.com/ggerganov/whisper.cpp) with support for Intel OpenVINO acceleration.

Designed for offline, private transcription on local hardware — no cloud APIs, no data leaves your machine.

## Features

- Transcribe audio files (wav, mp3, flac, ogg, m4a, aac, wma)
- Transcribe video files directly (mp4, mkv, avi, webm, mov) via automatic audio extraction
- Multiple output formats: plain text, SRT subtitles, JSON with timestamps, RAG-ready chunks
- Batch processing: transcribe entire directories in one command
- Watch mode: monitor a folder and auto-transcribe new files as they appear
- OpenVINO acceleration for Intel CPUs and GPUs (encoder speedup)

## Requirements

- CMake 3.14+
- C++17 compiler
- FFmpeg (for video/non-wav audio support)

```bash
# macOS
brew install cmake ffmpeg
```

## Build

```bash
# clone with submodule
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

# or if already cloned
git submodule update --init --recursive

# build
cmake -B build
cmake --build build -j

# download a model
cd models
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
cd ..
```

### Build with OpenVINO (Intel hardware)

```bash
# source openvino environment
source /opt/intel/openvino/setupvars.sh

# convert whisper encoder to openvino
cd third_party/whisper.cpp/models
python3 convert-whisper-to-openvino.py --model base.en
cd ../../..

# build with openvino flag
cmake -B build -DWHISPER_OPENVINO=1
cmake --build build -j
```

## Usage

```bash
# transcribe an audio file
./build/lisper -m models/ggml-base.en.bin recording.wav

# transcribe a video file
./build/lisper -m models/ggml-base.en.bin lecture.mp4

# generate SRT subtitles
./build/lisper -m models/ggml-base.en.bin lecture.mp4 -f srt -o lecture.srt

# JSON output with timestamps
./build/lisper -m models/ggml-base.en.bin podcast.mp3 -f json -o output.json

# RAG-ready chunked output (for knowledge base integration)
./build/lisper -m models/ggml-base.en.bin lecture.mp4 -f rag -o ./knowledge_base/

# batch transcribe a folder
./build/lisper -m models/ggml-base.en.bin -d ./recordings/ -o ./transcripts/

# watch mode - auto-transcribe new files
./build/lisper -m models/ggml-base.en.bin -w ./inbox/ -o ./transcripts/ -f srt
```

## Output Formats

| Format | Flag | Description |
|--------|------|-------------|
| Text | `-f text` | Plain transcription text (default) |
| SRT | `-f srt` | SubRip subtitle format with timestamps |
| JSON | `-f json` | Structured output with segment timestamps |
| RAG | `-f rag` | Chunked text for vector database ingestion |

## Architecture

```
src/
  main.cpp         CLI entry point with argument parsing
  lisper.h/.cpp    Core transcription engine (reusable as library)
  media.h/.cpp     Audio/video file handling + FFmpeg extraction
  formatter.h/.cpp Output formatting (text, srt, json, rag)
  watcher.h/.cpp   Directory watch mode for auto-transcription
```

The core engine (`lisper.h`) is designed as a standalone library that can be integrated into GUI applications or other projects.

## License

MIT
