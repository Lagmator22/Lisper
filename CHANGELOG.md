# Changelog

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