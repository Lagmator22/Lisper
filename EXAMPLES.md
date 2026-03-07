# Lisper Usage Examples

## Basic Usage

### Simple Transcription
Transcribe a single audio file to text:
```bash
./build/lisper --model-profile quality audio.wav
```

### Video Transcription
Extract and transcribe audio from a video file:
```bash
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin video.mp4
```

## Output Formats

### Generate SRT Subtitles
Create subtitle files for videos:
```bash
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin lecture.mp4 -f srt -o lecture.srt
```

### JSON Output with Timestamps
Get structured data with segment timestamps:
```bash
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin podcast.mp3 -f json -o output.json
```

### RAG-Ready Chunks
Split transcription into chunks for vector database ingestion:
```bash
./build/lisper --model-profile quality lecture.mp4 -f rag -o ./knowledge_base/
```

## Batch Processing

### Process Entire Directory
Transcribe all media files in a folder:
```bash
./build/lisper --model-profile quality -d ./recordings/ -o ./transcripts/
```

### Generate Subtitles for Multiple Videos
```bash
./build/lisper --model-profile quality -d ./videos/ -o ./subtitles/ -f srt
```

## Watch Mode

### Auto-transcribe New Files
Monitor a directory and automatically transcribe new files as they appear:
```bash
./build/lisper --model-profile quality -w ./inbox/ -o ./transcripts/ -f srt
```

This is useful for:
- Automated workflow integration
- Processing files from recording software
- Monitoring download folders

## Advanced Options

### Multi-language Support
Transcribe non-English audio (requires multilingual model):
```bash
./build/lisper -m models/ggml-base.bin audio.wav -l es  # Spanish
./build/lisper -m models/ggml-base.bin audio.wav -l fr  # French
./build/lisper -m models/ggml-base.bin audio.wav -l de  # German
```

### Translation Mode
Translate foreign language to English (requires multilingual model):
```bash
./build/lisper -m models/ggml-base.bin spanish_audio.wav --translate
```

### Performance Tuning
Adjust thread count for faster processing:
```bash
./build/lisper --model-profile quality audio.wav -t 8  # Use 8 threads

# Force CPU backend (macOS/Windows)
./build/lisper --model-profile quality --device cpu audio.wav
```

## Live Microphone Transcription
If compiled with SDL2 support:
```bash
./build/lisper --model-profile quality --live
```

## Common Workflows

### Subtitle Generation Pipeline
1. Process all videos in a folder
2. Generate SRT files
3. Review and edit if needed

```bash
# Batch process videos
./build/lisper --model-profile quality -d ./raw_videos/ -o ./subtitles/ -f srt

# Review generated subtitles
ls ./subtitles/*.srt
```

### Meeting Transcription Workflow
1. Record meeting audio
2. Transcribe to text
3. Generate summary chunks for notes

```bash
# Transcribe meeting
./build/lisper --model-profile quality meeting.mp3 -o meeting.txt

# Generate RAG chunks for searchable notes
./build/lisper --model-profile quality meeting.mp3 -f rag -o ./meeting_notes/
```

### Podcast Processing
1. Watch podcast download folder
2. Auto-generate transcripts
3. Create searchable archive

```bash
# Start watching downloads folder
./build/lisper --model-profile quality -w ~/Downloads/podcasts/ -o ./transcripts/ -f json
```

## Tips and Best Practices

1. **Model Selection**: Use larger models for better accuracy, smaller models for speed
   - `tiny.en` - Fastest, lowest accuracy
   - `base.en` - Good balance
   - `small.en` - Better accuracy
   - `medium.en` - High accuracy
   - `large-v3-turbo-q5_0` - Recommended quality profile
   - `large-v3-turbo` - Highest quality, highest memory use

2. **Audio Quality**: For best results:
   - Use clear audio without background noise
   - Ensure proper recording levels
   - Consider preprocessing with noise reduction tools

3. **File Organization**:
   - Keep raw media separate from transcripts
   - Use consistent naming conventions
   - Organize by date or project

4. **Performance**:
   - Increase thread count on multi-core systems
   - Use OpenVINO acceleration on Intel hardware
   - Process in batches during off-hours

5. **Integration**:
   - Use watch mode for automated workflows
   - Export to JSON for integration with other tools
   - Generate RAG chunks for knowledge bases
