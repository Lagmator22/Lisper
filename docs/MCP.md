# Lisper MCP Server

`lisper-mcp` exposes Lisper's on-device speech-to-text to any
[Model Context Protocol](https://modelcontextprotocol.io) client: Claude Desktop,
Claude Code, Cursor, VS Code, Windsurf, Zed and others. It speaks JSON-RPC 2.0
over stdio (protocol versions `2025-06-18`, `2025-03-26`, `2024-11-05`).

Audio never leaves the machine. The server only reads files you allow, and the
microphone, dictation history and network downloads are off unless you turn
them on.

## Install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build          # installs lisper, lisper-mcp (and lisper_gui)

# Download a model once (stored in your per-user models folder)
lisper --model-profile quality --download-model
```

Model files live in:

| OS      | Models directory                                   |
|---------|----------------------------------------------------|
| macOS   | `~/Library/Application Support/Lisper/models`      |
| Windows | `%APPDATA%\Lisper\models`                          |
| Linux   | `~/.local/share/lisper/models`                     |

Set `LISPER_MODELS_DIR` to use a different folder.

## Configure your client

### Claude Code

```bash
claude mcp add lisper -- lisper-mcp --allow-dir ~/Recordings --allow-microphone
```

### Claude Desktop

Edit `claude_desktop_config.json` (Settings → Developer → Edit Config):

```json
{
  "mcpServers": {
    "lisper": {
      "command": "/usr/local/bin/lisper-mcp",
      "args": ["--allow-dir", "/Users/you/Recordings", "--allow-microphone"]
    }
  }
}
```

### Cursor / VS Code / other clients

Any client that launches stdio servers works the same way: the command is the
full path to `lisper-mcp`, and the arguments are the flags below.

## Options

| Flag | Effect |
|------|--------|
| `--allow-dir <dir>` | Directory `transcribe_file` may read. Repeat for several. Default: your home directory. Paths are canonicalised, so `..` and symlinks cannot escape. |
| `--allow-microphone` | Adds `record_and_transcribe`. |
| `--allow-history` | Adds `get_dictation_history` (reads the desktop app's dictation history). |
| `--allow-download` | Adds `download_model` (HTTPS download from Hugging Face). |
| `--model-profile <name>` | `tiny`, `fast`, `balanced`, `quality`, `max`. |
| `-m, --model <path>` | Explicit GGML model file. |
| `-l, --language <code>` | Default language (`auto` to detect). |
| `-t, --threads <n>` | Decoder threads. |

Defaults not set by flags (model profile, language, personal dictionary,
snippets, cleanup style) come from the desktop app's `settings.json`, so the
MCP server and the dictation app spell names the same way.

## Tools

| Tool | Description |
|------|-------------|
| `transcribe_file` | Transcribe a local audio/video file. Args: `path`, optional `language`, `translate`, `format` (`text`/`srt`/`json`), `cleanup`. Returns text plus `structuredContent` with timestamped segments. |
| `transcribe_audio` | Transcribe base64 audio sent in the request. Args: `data`, `extension` (`wav`, `mp3`, `m4a`, `webm`, ...), plus the options above. 50 MB limit. |
| `record_and_transcribe` | *(opt-in)* Record the microphone for `seconds` (1–120) and return the cleaned transcript. |
| `clean_text` | Run Lisper's dictation cleanup (filler words, "new line" commands, dictionary, snippets) on text. |
| `list_models` | Model profiles, which are installed, and which is active. |
| `download_model` | *(opt-in)* Download a profile's model. |
| `get_dictation_history` | *(opt-in)* Recent dictations, newest first, with optional `query` filter. |

Requests can be cancelled with `notifications/cancelled`; a running
transcription stops at the next decoder checkpoint.

## Security model

- **No shell.** ffmpeg and curl are launched with argument vectors, never
  through a shell, so file names cannot inject commands.
- **ffmpeg is restricted to local files.** Inputs are passed as `file:` URLs
  with a `file`-only protocol whitelist, so names such as `http://…` or
  `concat:…` cannot make ffmpeg fetch remote or unintended data.
- **Filesystem sandbox.** `transcribe_file` only reads existing media files
  inside the allowed directories, after resolving symlinks and `..`.
- **Private data is opt-in.** Microphone, history and downloads are disabled
  unless their flag is given.
- **Bounded input.** Base64 payloads are size-limited; temporary files are
  created exclusively with owner-only permissions and deleted afterwards.
- **Clean protocol stream.** The server moves stray library output to stderr
  so it can never corrupt stdout.

## Manual test

```bash
printf '%s\n' \
 '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}}' \
 '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' | lisper-mcp
```

Or use the MCP Inspector: `npx @modelcontextprotocol/inspector lisper-mcp`.
