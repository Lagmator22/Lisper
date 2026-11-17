# Desktop App Architecture

The desktop app (`lisper_gui`) is a dictation-first tool in the style of Wispr
Flow: it lives in the background, listens when you press a shortcut, and types
cleaned-up text into whatever app has focus. File transcription is a secondary
page.

## Stack

- **SDL2:** window, renderer, audio capture and playback, clipboard, shaped
  overlay window.
- **Dear ImGui + SDL_Renderer:** the main window UI.
- **Plain SDL_RenderGeometry:** the floating bar (no ImGui), so it can live in
  its own small window.
- **lisper_core:** engine, text cleanup, settings, history, shared with the CLI
  and the MCP server.

## Threads

| Thread | Work |
|---|---|
| UI (main) | Events, rendering, clipboard and paste sequencing, settings saves |
| Model loader | Loads whisper weights when the model or device changes |
| SDL audio | Microphone callback: appends samples and level meter values |
| Decoder | One per dictation: whisper decode + text cleanup |
| File job | `BackgroundTranscriber`: ffmpeg + decode for the Transcribe page |
| Hotkey | Windows message loop / X11 event loop for the global shortcut |
| Control socket | Accepts `toggle` / `show` / `start` / `stop` from `lisper_gui --toggle` |
| Download | curl model download |

Background threads never touch UI state directly; they post SDL user events
(`SDL_PushEvent` is thread-safe) or update state behind a mutex that the UI
polls. Cancellation is per job (`DecodeOptions::cancel`), so cancelling a file
transcription never aborts a dictation.

## Dictation flow

```
shortcut ─▶ Dictation::start ─▶ AudioCapture (16 kHz mono float)
                                        │ levels ─▶ Overlay waveform
shortcut ─▶ Dictation::stop ─▶ decoder thread
                                        │ whisper (initial prompt = vocabulary)
                                        │ text_cleanup (fillers, commands, dictionary, snippets, style)
                                        ▼
UI thread: clipboard ← text ─▶ paste keystroke (CGEventPost / SendInput / wtype / xdotool)
           ─▶ restore previous clipboard ─▶ history.jsonl
```

If Lisper's own window has focus when the text is ready, it only copies to the
clipboard instead of pasting into itself.

## Files

| File | Responsibility |
|---|---|
| `gui_app.cpp` | App shell: CLI flags, single instance, event loop, insertion, persistence |
| `gui/dictation.*` | Dictation state machine, model loading, decoding |
| `gui/overlay.*` | Floating bar window |
| `gui/hotkey.*` | Global shortcut per platform, shortcut parsing |
| `gui/text_insert.*` | Paste keystroke per platform, permission checks |
| `gui/control_channel.*` | Single-instance Unix socket |
| `gui/sounds.*` | Synthesized start/stop/error chimes |
| `gui/layout.*` | Pages and sidebar |
| `gui/theme.*` | Light/dark palettes and widgets (cards, toggles, segmented controls, keycaps, icons) |
| `gui/state.*` | UI state and file-job request building |
| `gui/background_transcriber.*` | File transcription worker |
