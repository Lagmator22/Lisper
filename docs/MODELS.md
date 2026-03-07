# Whisper Model Guide for Lisper

This guide helps you choose the right Whisper model for your transcription needs and understand the trade-offs between speed, accuracy, and resource usage.

## Table of Contents

- [Quick Recommendations](#quick-recommendations)
- [Available Models](#available-models)
- [Model Profiles](#model-profiles)
- [Downloading Models](#downloading-models)
- [Performance Comparison](#performance-comparison)
- [Language Support](#language-support)
- [Optimization Tips](#optimization-tips)
- [Model Formats](#model-formats)

## Quick Recommendations

| Use Case | Recommended Model | Profile | Why |
|----------|------------------|---------|-----|
| Real-time transcription | `ggml-small-q5_1.bin` | `fast` | Low latency, good accuracy |
| General use | `ggml-large-v3-turbo-q5_0.bin` | `quality` | Best balance of speed and accuracy |
| Podcast/Interview | `ggml-large-v3-turbo-q5_0.bin` | `quality` | Handles multiple speakers well |
| Legal/Medical | `ggml-large-v3.bin` | `max` | Maximum accuracy for critical content |
| Non-English | `ggml-large-v3-turbo-q5_0.bin` | `quality` | Best multilingual support |
| Low-end hardware | `ggml-tiny-q5_1.bin` | - | Minimal resource usage |
| Batch processing | `ggml-medium-q5_0.bin` | `balanced` | Good throughput |

## Available Models

### Model Sizes and Characteristics

| Model | Original Size | Quantized Size | Parameters | Relative Speed | English | Multilingual |
|-------|--------------|----------------|------------|----------------|---------|--------------|
| tiny | 39 MB | ~18 MB | 39M | 10x | ✓ | ✓ |
| tiny.en | 39 MB | ~18 MB | 39M | 10x | ✓ | ✗ |
| base | 74 MB | ~42 MB | 74M | 7x | ✓ | ✓ |
| base.en | 74 MB | ~42 MB | 74M | 7x | ✓ | ✗ |
| small | 244 MB | ~190 MB | 244M | 4x | ✓ | ✓ |
| small.en | 244 MB | ~190 MB | 244M | 4x | ✓ | ✗ |
| medium | 769 MB | ~514 MB | 769M | 2x | ✓ | ✓ |
| medium.en | 769 MB | ~514 MB | 769M | 2x | ✓ | ✗ |
| large-v1 | 1550 MB | ~1074 MB | 1550M | 1x | ✓ | ✓ |
| large-v2 | 1550 MB | ~1074 MB | 1550M | 1x | ✓ | ✓ |
| large-v3 | 1550 MB | ~1074 MB | 1550M | 1x | ✓ | ✓ |
| large-v3-turbo | 809 MB | ~574 MB | 809M | 1.5x | ✓ | ✓ |

### Quantization Levels

Quantization reduces model size and improves speed with minimal accuracy loss:

| Quantization | Size Reduction | Speed Improvement | Accuracy Impact |
|--------------|---------------|-------------------|-----------------|
| f16 | ~50% | ~1.2x | Negligible |
| q8_0 | ~66% | ~1.5x | Very small |
| q5_1 | ~70% | ~1.8x | Small |
| q5_0 | ~72% | ~2x | Small |
| q4_1 | ~75% | ~2.2x | Moderate |
| q4_0 | ~77% | ~2.5x | Moderate |

## Model Profiles

Lisper includes pre-configured profiles that automatically select appropriate models:

### Fast Profile
```bash
./build/lisper --model-profile fast audio.wav
```
- **Model**: `ggml-small-q5_1.bin`
- **Use Cases**: Quick drafts, real-time transcription, live captioning
- **Speed**: 4x faster than large models
- **Accuracy**: Good for clear speech
- **RAM Usage**: ~300-400 MB

### Balanced Profile
```bash
./build/lisper --model-profile balanced audio.wav
```
- **Model**: `ggml-medium-q5_0.bin`
- **Use Cases**: Daily transcription, general content
- **Speed**: 2x faster than large models
- **Accuracy**: Very good for most content
- **RAM Usage**: ~600-800 MB

### Quality Profile (Recommended)
```bash
./build/lisper --model-profile quality audio.wav
```
- **Model**: `ggml-large-v3-turbo-q5_0.bin`
- **Use Cases**: Professional transcription, multiple speakers
- **Speed**: 1.5x faster than large-v3
- **Accuracy**: Excellent, near-maximum quality
- **RAM Usage**: ~800-1200 MB

### Max Profile
```bash
./build/lisper --model-profile max audio.wav
```
- **Model**: `ggml-large-v3.bin`
- **Use Cases**: Critical accuracy, legal/medical content
- **Speed**: Baseline (slowest)
- **Accuracy**: Maximum possible
- **RAM Usage**: ~1500-2000 MB

## Downloading Models

### Automatic Download (Recommended)

```bash
# Download via whisper.cpp script
cd third_party/whisper.cpp/models
./download-ggml-model.sh large-v3-turbo-q5_0
mv ggml-large-v3-turbo-q5_0.bin ../../../models/
```

### Manual Download

```bash
# Create models directory
mkdir -p models
cd models

# Download specific models from Hugging Face
# Small (fast transcription)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin

# Medium (balanced)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium-q5_0.bin

# Large v3 Turbo (recommended)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin

# Large v3 (maximum accuracy)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin
```

### Converting Models

If you have PyTorch models, convert them to GGML format:

```bash
cd third_party/whisper.cpp/models
python3 convert-pt-to-ggml.py ~/.cache/whisper/large-v3.pt .
./quantize ggml-large-v3.bin ggml-large-v3-q5_0.bin q5_0
```

## Performance Comparison

### Speed Benchmarks (Apple M2, 8GB RAM)

| Model | Audio Duration | Processing Time | Real-time Factor | RAM Usage |
|-------|---------------|-----------------|------------------|-----------|
| tiny-q5_1 | 60s | 3s | 20x | 150 MB |
| base-q5_1 | 60s | 5s | 12x | 200 MB |
| small-q5_1 | 60s | 8s | 7.5x | 400 MB |
| medium-q5_0 | 60s | 15s | 4x | 800 MB |
| large-v3-turbo-q5_0 | 60s | 20s | 3x | 1200 MB |
| large-v3 | 60s | 30s | 2x | 2000 MB |

### Accuracy Comparison (Word Error Rate)

| Model | Clean Speech | Noisy Audio | Multiple Speakers | Non-Native |
|-------|-------------|-------------|-------------------|------------|
| tiny | 8.5% | 15% | 18% | 20% |
| base | 6.5% | 11% | 14% | 16% |
| small | 4.5% | 8% | 10% | 12% |
| medium | 3.2% | 6% | 7% | 9% |
| large-v3-turbo | 2.5% | 4.5% | 5% | 7% |
| large-v3 | 2.1% | 4% | 4.5% | 6% |

## Language Support

### English-Only Models

English-only models (`.en` variants) are faster and more accurate for English content:

```bash
# Better for English-only content
./build/lisper -m models/ggml-base.en.bin english_audio.wav
```

### Multilingual Models

Standard models support 100+ languages:

```bash
# Specify language for better accuracy
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin audio.wav -l ja  # Japanese
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin audio.wav -l es  # Spanish
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin audio.wav -l fr  # French

# Auto-detect language (slightly slower)
./build/lisper -m models/ggml-large-v3-turbo-q5_0.bin audio.wav -l auto
```

### Supported Languages (ISO 639-1 codes)

Common languages with excellent support:
- `en` - English
- `zh` - Chinese
- `es` - Spanish
- `fr` - French
- `de` - German
- `ja` - Japanese
- `ko` - Korean
- `ru` - Russian
- `ar` - Arabic
- `hi` - Hindi

[Full list of 100+ supported languages](https://github.com/openai/whisper/blob/main/whisper/tokenizer.py)

## Optimization Tips

### For Speed

1. **Use quantized models**: Q5_0 or Q5_1 offer best speed/quality balance
2. **Adjust threads**: Use `-t` flag to match CPU cores
3. **Use smaller models**: Start with small, only use large if needed
4. **Enable GPU**: Use `--device gpu` if available
5. **Disable beam search**: Faster but slightly less accurate

```bash
# Optimized for speed
./build/lisper -m models/ggml-small-q5_1.bin audio.wav -t 8 --device gpu
```

### For Accuracy

1. **Use larger models**: large-v3 or large-v3-turbo
2. **Specify language**: Use `-l` flag instead of auto-detection
3. **Clean audio**: Preprocess to remove noise
4. **Adjust temperature**: Lower values for more conservative transcription

```bash
# Optimized for accuracy
./build/lisper -m models/ggml-large-v3.bin audio.wav -l en
```

### For Memory-Constrained Systems

1. **Use tiny or base models**
2. **Enable memory mapping**: Loads model on-demand
3. **Reduce threads**: Lower thread count reduces memory usage
4. **Use quantized models**: Q4_0 for minimum memory

```bash
# Optimized for low memory
./build/lisper -m models/ggml-tiny-q4_0.bin audio.wav -t 2
```

## Model Formats

### GGML Format

Lisper uses GGML format for models:
- **Extension**: `.bin`
- **Advantages**: Fast loading, memory-mapped, CPU-optimized
- **Naming**: `ggml-[size]-[variant]-[quantization].bin`

### OpenVINO Format

For Intel hardware acceleration:
```bash
# Convert to OpenVINO
cd third_party/whisper.cpp/models
python3 convert-whisper-to-openvino.py --model base.en

# Use OpenVINO model
./build/lisper -m models/ggml-base.en-encoder-openvino.xml audio.wav
```

### CoreML Format (macOS)

For Apple Neural Engine:
```bash
# Convert to CoreML
cd third_party/whisper.cpp/models
./convert-whisper-to-coreml.py --model base.en

# Build with CoreML support
cmake -B build -DWHISPER_COREML=ON
```

## Advanced Configuration

### Custom Model Paths

```bash
# Use absolute path
./build/lisper -m /path/to/custom/model.bin audio.wav

# Use relative path
./build/lisper -m ../models/custom-model.bin audio.wav

# Set model directory environment variable
export LISPER_MODEL_PATH=/custom/model/directory
./build/lisper --model-profile quality audio.wav
```

### Model Validation

Check if a model works correctly:
```bash
# Quick validation
./build/lisper -m models/ggml-base.en.bin --validate

# Benchmark model
./build/lisper -m models/ggml-base.en.bin --benchmark
```

## Troubleshooting

### Common Issues

**"Model not found"**
- Check file exists: `ls -la models/`
- Verify path is correct
- Ensure model is fully downloaded (check file size)

**"Insufficient memory"**
- Use smaller or more quantized model
- Reduce thread count with `-t 2`
- Close other applications

**"Poor transcription quality"**
- Try larger model
- Specify correct language with `-l`
- Check audio quality (sample rate, noise)

**"Slow transcription"**
- Use quantized model (q5_0 or q5_1)
- Enable GPU if available
- Increase thread count
- Use smaller model for non-critical content

## Model Selection Flowchart

```
Start
  ↓
Is real-time needed? → Yes → Use small-q5_1
  ↓ No
Is English-only? → Yes → Use .en variant
  ↓ No
Is accuracy critical? → Yes → Use large-v3
  ↓ No
Is speed important? → Yes → Use large-v3-turbo-q5_0
  ↓ No
Use medium-q5_0 (balanced default)
```

## Updates and New Models

Check for new models and updates:
- [Whisper.cpp Models](https://github.com/ggerganov/whisper.cpp/tree/master/models)
- [Hugging Face Repository](https://huggingface.co/ggerganov/whisper.cpp)
- [OpenAI Whisper Releases](https://github.com/openai/whisper/releases)