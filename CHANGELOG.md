# Changelog

## [0.3.0] - 2026-11-17

### Dictation app (Wispr Flow style)
- System-wide dictation shortcut (default Ctrl+Shift+Space), toggle or hold-to-talk,
  on Windows, macOS and X11; `lisper_gui --toggle` for Wayland and scripts.
- Floating bar with live waveform, transcribing and done/error states.
- Text is cleaned up and pasted into the focused app, with clipboard restore,
  or copied only.
- Filler-word removal, "new line"/"new paragraph" voice commands, formal/casual style.
- Personal dictionary (replacements + vocabulary prompt) and snippets.
- Local history with search, copy and delete; stats for words, WPM, streak and time saved.
- Redesigned app: Home, Transcribe files, Dictionary, Snippets, Style, Settings;
  light and dark themes; start/stop chimes; in-app model downloads.
- Single instance; closing keeps Lisper running in the background (configurable).
- macOS app bundle with microphone usage description; Linux desktop entry.

### MCP server
- New `lisper-mcp` (stdio, MCP 2025-06-18/2025-03-26/2024-11-05) with
  `transcribe_file`, `transcribe_audio`, `clean_text`, `list_models`, and opt-in
  `record_and_transcribe`, `get_dictation_history`, `download_model`.
- Directory allow-list, request cancellation, size limits. See docs/MCP.md.

### Security
- All external programs run without a shell (`posix_spawnp` / `CreateProcessW`).
- ffmpeg restricted to local files (`file:` URLs + protocol whitelist).
- Exclusive, owner-only temporary files with unpredictable names.
- Fixed 32-bit overflow in WAV chunk validation; JSON output escapes all control characters.
- Model download is opt-in (`--download-model`), HTTPS-only, validated and atomic,
  replacing execution of a shell script from the working directory.
- Settings and history stored with owner-only permissions.

### Engine and CLI
- `--language auto` reports the detected language.
- WAVs that are not 16 kHz 16-bit PCM are resampled instead of transcribed at the wrong rate.
- Thread-safe engine with per-call language/translate overrides and cancellation.
- Watch mode waits for files to finish copying.
- Per-user models directory (`LISPER_MODELS_DIR`, `LISPER_HOME`); new `tiny` profile.

### Build
- `LISPER_ENABLE_LIVE`, `LISPER_BUILD_TESTS` options; install rules; unit tests in CI;
  end-to-end CLI and MCP checks in CI.
- Removed dead code: unused watcher APIs, legacy GUI state, unused theme helpers.

## [0.2.0] - 2024-03-06

### Security Fixes
- Fixed command injection vulnerability in media.cpp by properly escaping shell special characters in file paths
- Added input validation to prevent buffer overflows in WAV parser
- Improved memory safety with bounds checking and exception handling

### Features
- Added signal handling for graceful shutdown (Ctrl+C) during batch processing and watch mode
- Improved progress indication for batch processing with time estimates
- Made SDL2 dependency optional - live transcription disabled if SDL2 not found
- Added case-insensitive file extension matching for better cross-platform compatibility
- Added RAII wrapper for automatic temporary file cleanup

### Bug Fixes
- Fixed potential memory issues in WAV file parser
- Added input validation for thread count parameter
- Improved error handling for invalid or corrupted WAV files
- Fixed temporary file cleanup on error conditions

### Improvements
- Enhanced batch processing output with success/failure tracking
- Added better error messages for invalid inputs
- Improved WAV parser robustness with file size validation
- Added protection against infinite loops in chunk parsing
- Better memory management with exception safety

## [0.1.0] - Initial Release

### Features
- Basic audio and video file transcription
- Support for multiple audio formats (WAV, MP3, FLAC, OGG, M4A, AAC, WMA)
- Support for video formats (MP4, MKV, AVI, WebM, MOV, FLV, WMV, M4V)
- Multiple output formats (text, SRT, JSON, RAG)
- Batch processing mode
- Directory watch mode
- FFmpeg integration for audio extraction
- whisper.cpp integration for transcription