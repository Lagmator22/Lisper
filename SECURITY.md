# Security Policy

## Reporting a vulnerability

Please report security issues privately through
[GitHub Security Advisories](https://github.com/Lagmator22/Lisper/security/advisories/new)
rather than in a public issue. Include the version, platform and steps to
reproduce. You can expect an acknowledgement within a few days.

## Threat model

Lisper handles two sensitive things: the user's **voice** and the **text they
dictate**. The design goal is that neither leaves the machine and that no input
file, setting or MCP request can make Lisper run arbitrary commands or read
files it was not asked to read.

| Area | Protection |
|---|---|
| Speech data | Transcribed locally by whisper.cpp. Audio is kept in memory only for the duration of a dictation and never written to disk or sent anywhere. |
| Network | The only outbound connection is a user-initiated model download: `curl --proto =https --proto-redir =https --tlsv1.2` to Hugging Face. The file is checked for the GGML header and renamed into place atomically. |
| External processes | ffmpeg, curl, xdotool and wtype are started with `posix_spawnp` / `CreateProcessW` and an argument vector. No shell is involved, so file names cannot inject commands. |
| ffmpeg input | Inputs are passed as `file:` URLs with `-protocol_whitelist file`, so names like `http://…`, `concat:…` or `-` cannot make ffmpeg fetch remote or unintended data. |
| Temporary files | Created with `O_CREAT|O_EXCL|O_NOFOLLOW`, mode `0600`, 128-bit random names, and deleted after use. |
| Local data | Settings (`settings.json`) and history (`history.jsonl`) are written atomically with owner-only permissions in a `0700` directory. Values loaded from disk are type-checked and clamped. |
| Single-instance socket | A Unix socket inside the private data directory, accepting only `toggle`, `show`, `start`, `stop`. |
| WAV parsing | Bounds-checked RIFF parser with 64-bit size arithmetic, sample-count limits and chunk iteration caps. |
| MCP server | File access limited to allowed directories after canonicalisation (blocks `..`, symlink escapes and look-alike sibling paths) and to known media extensions. Microphone, history and downloads are opt-in flags. Base64 payloads are size-limited. Stray library output is redirected away from the protocol stream. |

## Platform permissions

- **macOS:** Microphone (to dictate) and Accessibility (to paste into other
  apps). Lisper works without Accessibility in clipboard-only mode.
- **Windows:** none beyond microphone privacy settings.
- **Linux:** none; pasting uses `xdotool` (X11) or `wtype` (Wayland).

## Supported versions

Security fixes are made on the latest release.
