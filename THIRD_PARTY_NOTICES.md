# Third-Party Notices

Lisper is MIT licensed. It includes or links the following components, all of
which permit commercial use. Their license texts ship in `third_party/` or with
the respective upstream package.

| Component | Use | License |
|---|---|---|
| [whisper.cpp / ggml](https://github.com/ggerganov/whisper.cpp) | Speech recognition engine (git submodule) | MIT, Copyright (c) 2023-2026 The ggml authors |
| [Dear ImGui](https://github.com/ocornut/imgui) | Desktop UI | MIT, Copyright (c) 2014-2026 Omar Cornut |
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | JSON for settings, history and MCP | MIT, Copyright (c) 2013-2022 Niels Lohmann |
| [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) | Native file pickers | zlib, Copyright (c) 2014-2017 Guillaume Vareille |
| [SDL2](https://libsdl.org) | Windowing, audio capture, clipboard (linked) | zlib, Copyright (C) 1997-2024 Sam Lantinga |

## Runtime tools (not distributed with Lisper)

| Tool | Use | License |
|---|---|---|
| [FFmpeg](https://ffmpeg.org) | Decoding non-WAV audio and video, run as a separate process | LGPL/GPL depending on build |
| [curl](https://curl.se) | Model downloads, run as a separate process | curl license (MIT-style) |
| xdotool / wtype | Pasting on Linux, run as separate processes | BSD-3-Clause / MIT |

Because FFmpeg and curl are invoked as separate executables and not linked,
their licenses do not apply to Lisper's own code. If you bundle an FFmpeg build
with a commercial distribution, follow the license of that particular build
(an LGPL build is usually the right choice).

## Models

Whisper model weights by OpenAI are released under the MIT license. The GGML
conversions are downloaded from
[huggingface.co/ggerganov/whisper.cpp](https://huggingface.co/ggerganov/whisper.cpp)
on request and are not part of this repository.
