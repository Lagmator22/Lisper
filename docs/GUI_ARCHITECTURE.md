# GUI Architecture

## Research Inputs

This redesign used a few concrete desktop references:

- Wispr Flow: minimal dictation-first framing and low-friction interaction model.
- Whisper4Windows: focused settings panel plus a restrained processing-wave animation.
- GitHub Desktop: reliable setup/navigation on the left with a document-style work area on the right.
- Microsoft PowerToys: responsive settings surfaces, consistent spacing, and small-window resilience.

## Chosen Stack

The GUI stack stays native to this repository:

- `SDL2`
  Cross-platform desktop window, input, drag-drop, and renderer setup already fits the existing project.
- `Dear ImGui` + `SDL_Renderer`
  Fast to ship in C++, easy to theme, and good for stable operator-style desktop tooling.
- `tinyfiledialogs`
  Adds native file and folder pickers without forcing a larger framework migration.
- Existing `lisper_core`
  The GUI remains a thin app layer over the same transcription engine the CLI uses.

This is intentionally not a Qt/Electron/Tauri rewrite. Those stacks can produce polished apps, but they would replace too much of the current build/runtime model to be the right move for this repository right now.

## UI Structure

The design follows a three-zone layout:

1. Header
   Large LISPER identity, status chips, and a processing-wave animation.
2. Setup Sidebar
   Input, output, model/device, and run controls in one scrollable configuration surface.
3. Workspace
   Transcript preview, activity log, and diagnostics in a separate tabbed work area.

This matches the broad pattern used by strong desktop apps: configuration stays isolated, the active surface stays uncluttered, and motion only indicates state changes.

## File Structure

The GUI is split under `src/gui/`:

- `state.h/.cpp`
  App state, formatting helpers, profile/device mapping, validation, and request building.
- `theme.h/.cpp`
  Color system, fonts, surfaces, pills, buttons, background drawing, and the processing wave.
- `background_transcriber.h/.cpp`
  Background job runner and lifecycle management around the existing transcription engine.
- `file_dialog.h/.cpp`
  Native input/output pickers via `tinyfiledialogs`.
- `layout.h/.cpp`
  Header, sidebar, workspace, tabs, and responsive layout composition.
- `gui_app.cpp`
  SDL + ImGui bootstrapping, event loop, and top-level state orchestration.

## Next Extensions

If the GUI needs another pass after this split, the next improvements should be:

- native model downloader / model manager
- waveform or timeline preview for completed transcripts
- persistent recent-files history
- global hotkey / tray mode
