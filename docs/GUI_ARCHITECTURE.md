# Desktop App Architecture

The desktop app (`lisper_gui`) is a dictation-first tool in the style of Wispr
Flow: it lives in the background, listens when you press a shortcut, and types
cleaned-up text into whatever app has focus. File transcription is a secondary
page.

## Stack

- **SDL2:** window, renderer, audio capture and playback, clipboard, shaped
  overlay window.
- **Dear ImGui + SDL_Renderer:** the main window UI.
- **A second ImGui context:** the dictation island, rendered into its own
  small always-on-top window.
- **Platform glue (`gui/platform*.{cpp,mm}`):** notch geometry, window level
  above the menu bar and focus hand-back on macOS; non-activating windows on
  Windows; no-focus hints on X11.
- **Bundled fonts:** Inter and Source Serif 4, embedded at build time by
  `cmake/embed_fonts.cmake`.
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
| `gui/overlay.*` | The island: spring-animated capsule, live words, waveform, states |
| `gui/platform.*` | Notch/screen geometry, window level, focus hand-back (`platform_mac.mm` on macOS) |
| `gui/hotkey.*` | Global shortcuts per platform, modifier-chord watcher, parsing |
| `gui/text_insert.*` | Paste keystroke per platform, permission checks |
| `gui/control_channel.*` | Single-instance Unix socket |
| `gui/sounds.*` | Synthesized start/stop/error chimes |
| `gui/layout.*` | Pages and sidebar |
| `gui/theme.*` | Light/dark palettes and widgets (cards, toggles, segmented controls, keycaps, icons) |
| `gui/state.*` | UI state and file-job request building |
| `gui/background_transcriber.*` | File transcription worker |

## The island

The island is drawn in a fixed-size canvas window (560×200 pt) positioned at
the top centre of the screen under the mouse. Only the island's shape is
visible: on macOS the window is transparent; on Windows and X11 an SDL shaped
window whose mask follows the island every frame (SDL2 shaped windows cannot be
resized, so the canvas never is). Width and height are springs, so the island
grows out of the notch with a slight overshoot and collapses back into it.
With `LISPER_SIMULATE_NOTCH=WxH` the notch layout can be previewed on any
machine.

## Shortcuts and chords

Key shortcuts use the platform's hotkey API. Modifier-only chords (⌘⌥ by
default on macOS) are detected by polling the global modifier and key state
every 12 ms, which needs no extra permission. `ChordTracker` emits
*Pressed* when exactly the chord is down, *Released* on a clean release, and
*Cancelled* if any other key, modifier or mouse button is used meanwhile, so
⌘⌥Esc or ⌘⌥-drag never start a dictation. Toggle mode acts on a quick, clean
tap; hold mode starts on press and stops on release.

## Live preview

While recording, a worker re-transcribes the rolling microphone buffer (last
24 s) about every 0.6 s once at least 1.5 s of non-silent audio exists, and
publishes the cleaned text. Stopping cancels the in-flight preview before the
final, full-accuracy pass.
