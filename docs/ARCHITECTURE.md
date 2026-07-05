# How Lisper works (plain-language architecture doc)

Lisper turns audio and video files into text, entirely on your machine.
This doc walks through every file so you can find your way around fast.

## The one-sentence version

Lisper is a thin, careful C++ layer around **whisper.cpp** (the model that
does the actual speech recognition), plus a CLI, a desktop GUI, and helpers
for batch jobs, folder watching, and live microphone transcription.

## The pipeline (what happens to your file)

```
your file (mp4/mp3/wav/...)
   |
   v
media::prepare_audio()      "is this already a 16kHz WAV? if not,
   |                         run ffmpeg to convert it to one"
   v
Lisper::transcribe()        "load WAV bytes -> float samples -> whisper.cpp"
   |
   v
TranscriptionResult         text + timestamped segments
   |
   v
formatter::format_result()  text / srt / json / rag chunks
   |
   v
stdout or output file
```

## File-by-file tour (src/)

| File | What it does, simply |
|---|---|
| `main.cpp` | The CLI. Parses flags, picks a model, then runs single-file, batch (`-d`), watch (`-w`), or live (`--live`) mode. |
| `lisper.h/.cpp` | The engine. Loads the Whisper model, reads WAV files by hand (with strict validation), calls `whisper_full()`, collects segments. |
| `media.h/.cpp` | File-type detection by extension, and the ffmpeg conversion step. Any non-WAV input becomes a temporary 16kHz mono WAV. |
| `formatter.h/.cpp` | Turns results into plain text, SRT subtitles, JSON, or RAG chunks (~500-char pieces cut at sentence ends, ready for a vector DB). |
| `watcher.h/.cpp` | Polls a folder every few seconds. New files are transcribed automatically, but only after their size stops changing (so half-copied files are not grabbed). |
| `live.h/.cpp` | Microphone mode. SDL2 captures audio into a buffer; every ~2 seconds the buffer is transcribed and printed. Compiled only when SDL2 is available. |
| `model_profiles.h/.cpp` | Named model presets (fast / balanced / quality / max) and the search paths used to find the model file on disk. |
| `interrupt.h/.cpp` | One global "user pressed Ctrl+C" flag, set from a signal handler, checked by every loop and passed to whisper.cpp as an abort callback. |
| `cli_style.h/.cpp` | All the terminal eye candy: colors, gradients, badges, spinner, startup banner. `--no-color` and `--no-animate` switch it off. |

## The GUI (src/gui/)

`lisper_gui` is a desktop app built with Dear ImGui on SDL2.

| File | What it does |
|---|---|
| `gui_app.cpp` | Window + render loop + drag-and-drop handling. |
| `gui/layout.cpp` | The actual screens: model picker, file picker, options, log panel, transcript preview. |
| `gui/state.h/.cpp` | One `AppState` struct holding everything the UI shows. |
| `gui/background_transcriber.*` | Runs the transcription on a worker thread so the window never freezes. The UI polls `consume_completed()` each frame. Cancel works through the same Ctrl+C flag as the CLI. |
| `gui/theme.cpp` | Fonts and colors. |
| `gui/file_dialog.*` | Native open/save dialogs. |

The important design idea: **the GUI reuses the exact same engine code as
the CLI** (`lisper_core` static library). Nothing is duplicated.

## Safety decisions worth knowing

- **WAV parsing is defensive**: header sizes are checked against the real
  file size with overflow-safe math, sample counts are capped (12h max),
  and reads never exceed the allocated buffer.
- **ffmpeg runs through the shell**, so paths are quoted (POSIX-safe
  single-quote escaping) and paths containing control characters (or `"`
  and `%` on Windows) are rejected before any command is built.
- **JSON output escapes control characters**, so the output is always
  valid JSON no matter what the model emits.
- **Temp files are cleaned by RAII guards** even if a step throws.
- **Ctrl+C is cooperative everywhere**: the signal only sets a flag; loops
  and whisper.cpp itself check it and stop at a safe point.

## Building

```sh
cmake -B build && cmake --build build -j
./build/lisper --model-profile fast jfk.wav      # CLI
./build/lisper_gui                                # GUI
```

whisper.cpp and imgui live in `third_party/` (git submodules). SDL2 is
optional; without it you lose `--live` and the GUI, nothing else.
