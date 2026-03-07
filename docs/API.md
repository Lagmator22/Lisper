# Lisper API Documentation

This document describes how to use Lisper as a C++ library in your own applications.

## Table of Contents

- [Installation](#installation)
- [Basic Usage](#basic-usage)
- [API Reference](#api-reference)
- [Advanced Usage](#advanced-usage)
- [Examples](#examples)
- [Error Handling](#error-handling)
- [Threading and Safety](#threading-and-safety)

## Installation

### As a CMake Subdirectory

```cmake
# In your CMakeLists.txt
add_subdirectory(third_party/lisper)
target_link_libraries(your_app PRIVATE lisper)
```

### As a System Library

```bash
# Install Lisper system-wide
cd Lisper
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j
sudo cmake --install build

# In your CMakeLists.txt
find_package(Lisper REQUIRED)
target_link_libraries(your_app PRIVATE Lisper::lisper)
```

## Basic Usage

### Minimal Example

```cpp
#include <lisper/lisper.h>
#include <iostream>

int main() {
    // Configure the transcription engine
    LisperConfig config;
    config.model_path = "models/ggml-base.en.bin";
    config.language = "en";
    config.threads = 4;

    // Create engine instance
    Lisper engine(config);

    // Check if model loaded successfully
    if (!engine.is_loaded()) {
        std::cerr << "Failed to load model\n";
        return 1;
    }

    // Transcribe audio file
    auto result = engine.transcribe("audio.wav");

    if (result.success) {
        std::cout << "Transcription: " << result.text << "\n";
    } else {
        std::cerr << "Error: " << result.error << "\n";
    }

    return 0;
}
```

## API Reference

### Core Classes

#### `LisperConfig`

Configuration structure for the transcription engine.

```cpp
struct LisperConfig {
    // Model configuration
    std::string model_path;        // Path to GGML model file
    std::string language = "en";   // ISO 639-1 language code or "auto"
    bool translate = false;        // Translate to English

    // Performance settings
    int threads = 4;                // Number of CPU threads
    enum class Device {
        Auto, CPU, GPU
    } device = Device::Auto;       // Device preference
    int gpu_device = 0;             // GPU device index
    bool flash_attn = true;         // Enable flash attention

    // Processing options
    int max_duration_ms = 0;       // Max audio duration (0 = unlimited)
    float temperature = 0.0f;       // Sampling temperature (0 = greedy)
    int beam_size = 5;              // Beam search size

    // Advanced options
    bool suppress_blank = true;     // Suppress blank outputs
    bool suppress_non_speech = false; // Remove non-speech tokens
    float entropy_threshold = 2.4f;  // Entropy threshold
    float logprob_threshold = -1.0f; // Log probability threshold
};
```

#### `Lisper`

Main transcription engine class.

```cpp
class Lisper {
public:
    // Constructor
    explicit Lisper(const LisperConfig& config);

    // Check if model is loaded
    bool is_loaded() const;

    // Transcribe audio file
    TranscriptionResult transcribe(const std::string& audio_path);

    // Transcribe audio buffer (16kHz, mono, float32)
    TranscriptionResult transcribe_raw(const float* samples,
                                      size_t sample_count);

    // Get model information
    ModelInfo get_model_info() const;

    // Update configuration (requires reload)
    void set_config(const LisperConfig& config);
};
```

#### `TranscriptionResult`

Result structure returned by transcription methods.

```cpp
struct TranscriptionResult {
    bool success = false;           // Whether transcription succeeded
    std::string error;              // Error message if failed
    std::string text;               // Full transcription text
    std::vector<Segment> segments;  // Timestamped segments
    int duration_ms = 0;            // Audio duration in milliseconds
    std::string language;           // Detected/used language code
    float language_probability = 0; // Language detection confidence
};

struct Segment {
    int start_ms;                   // Start time in milliseconds
    int end_ms;                     // End time in milliseconds
    std::string text;               // Segment text
    float confidence = 0;           // Confidence score [0-1]
    std::vector<Token> tokens;     // Optional: token-level data
};

struct Token {
    int start_ms;
    int end_ms;
    std::string text;
    float probability;
    int id;                         // Token ID in vocabulary
};
```

### Utility Functions

#### Media Processing

```cpp
namespace media {
    // Prepare audio for transcription
    PrepareResult prepare_audio(const std::string& input_path);

    // Extract audio from video
    bool extract_audio(const std::string& video_path,
                      const std::string& output_wav);

    // Convert audio to WAV format
    bool convert_to_wav(const std::string& input_path,
                       const std::string& output_path,
                       int sample_rate = 16000);

    // Get media file duration
    int get_duration_ms(const std::string& file_path);
}

struct PrepareResult {
    bool success;
    std::string error;
    std::string wav_path;
    bool is_temp;                  // Whether file should be deleted
};
```

#### Output Formatting

```cpp
namespace formatter {
    enum class Format {
        Text,    // Plain text
        SRT,     // SubRip subtitles
        JSON,    // JSON with timestamps
        RAG      // Chunked for vector databases
    };

    // Format transcription result
    std::string format_result(const TranscriptionResult& result,
                              Format format,
                              const std::string& filename = "");

    // Write formatted output to file
    bool write_output(const TranscriptionResult& result,
                     Format format,
                     const std::string& output_path,
                     const std::string& source_name = "");
}
```

## Advanced Usage

### Streaming Transcription

```cpp
#include <lisper/lisper.h>
#include <lisper/stream.h>

class TranscriptionCallback : public ITranscriptionCallback {
public:
    void on_segment(const Segment& segment) override {
        std::cout << "[" << segment.start_ms << "ms] "
                  << segment.text << std::endl;
    }

    void on_progress(float progress) override {
        std::cout << "Progress: " << (progress * 100) << "%\r"
                  << std::flush;
    }
};

int main() {
    LisperConfig config;
    config.model_path = "models/ggml-base.en.bin";

    Lisper engine(config);
    TranscriptionCallback callback;

    // Enable streaming with callback
    engine.set_callback(&callback);

    // Transcribe with real-time updates
    auto result = engine.transcribe("long_audio.wav");

    return 0;
}
```

### Batch Processing

```cpp
#include <lisper/lisper.h>
#include <lisper/batch.h>
#include <vector>
#include <thread>

void batch_transcribe(const std::vector<std::string>& files) {
    // Create thread pool
    const int num_workers = std::thread::hardware_concurrency();
    BatchProcessor processor(num_workers);

    // Configure engine for each worker
    LisperConfig config;
    config.model_path = "models/ggml-base.en.bin";
    config.threads = 2;  // Threads per worker

    // Process files
    auto results = processor.process(files, config);

    // Handle results
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].success) {
            std::cout << files[i] << ": "
                     << results[i].text.substr(0, 50) << "...\n";
        } else {
            std::cerr << files[i] << ": "
                     << results[i].error << "\n";
        }
    }
}
```

### Custom Audio Input

```cpp
#include <lisper/lisper.h>
#include <vector>

// Custom audio loader
std::vector<float> load_custom_audio(const std::string& path) {
    // Your audio loading logic
    std::vector<float> samples;
    // ... load and convert to 16kHz mono float32 ...
    return samples;
}

int main() {
    LisperConfig config;
    config.model_path = "models/ggml-base.en.bin";

    Lisper engine(config);

    // Load custom audio
    auto samples = load_custom_audio("custom_format.aud");

    // Transcribe raw samples
    auto result = engine.transcribe_raw(samples.data(),
                                       samples.size());

    if (result.success) {
        std::cout << result.text << "\n";
    }

    return 0;
}
```

### Live Microphone Transcription

```cpp
#include <lisper/lisper.h>
#include <lisper/live.h>

int main() {
    LisperConfig config;
    config.model_path = "models/ggml-small-q5_1.bin";
    config.threads = 4;

    Lisper engine(config);

    // Start live transcription (requires SDL2)
    live::start_live_transcription(engine);

    return 0;
}
```

### Directory Watching

```cpp
#include <lisper/lisper.h>
#include <lisper/watcher.h>

int main() {
    LisperConfig config;
    config.model_path = "models/ggml-base.en.bin";

    Lisper engine(config);

    watcher::WatchConfig watch_config;
    watch_config.watch_dir = "./input";
    watch_config.output_dir = "./output";

    // Watch directory and auto-transcribe new files
    watcher::watch_directory(watch_config,
        [&engine](const std::string& path) {
            auto result = engine.transcribe(path);
            std::cout << "Transcribed: " << path << "\n";
        });

    return 0;
}
```

## Examples

### Complete Application Example

```cpp
#include <lisper/lisper.h>
#include <lisper/media.h>
#include <lisper/formatter.h>
#include <filesystem>
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

class TranscriptionApp {
private:
    Lisper engine;
    formatter::Format output_format;

public:
    TranscriptionApp(const std::string& model_path)
        : engine(create_config(model_path)),
          output_format(formatter::Format::SRT) {}

    static LisperConfig create_config(const std::string& model_path) {
        LisperConfig config;
        config.model_path = model_path;
        config.language = "auto";
        config.threads = std::thread::hardware_concurrency();
        config.device = LisperConfig::Device::Auto;
        return config;
    }

    bool process_file(const std::string& input_path) {
        std::cout << "Processing: " << input_path << "\n";

        // Prepare audio (handles video/audio conversion)
        auto prepared = media::prepare_audio(input_path);
        if (!prepared.success) {
            std::cerr << "Failed to prepare audio: "
                     << prepared.error << "\n";
            return false;
        }

        // Transcribe
        auto result = engine.transcribe(prepared.wav_path);

        // Clean up temp file if needed
        if (prepared.is_temp) {
            fs::remove(prepared.wav_path);
        }

        if (!result.success) {
            std::cerr << "Transcription failed: "
                     << result.error << "\n";
            return false;
        }

        // Format and save output
        std::string output_path = fs::path(input_path)
            .replace_extension(".srt").string();

        if (!formatter::write_output(result, output_format,
                                    output_path, input_path)) {
            std::cerr << "Failed to write output\n";
            return false;
        }

        std::cout << "Saved to: " << output_path << "\n";
        std::cout << "Duration: " << result.duration_ms / 1000
                  << "s, Segments: " << result.segments.size() << "\n";

        return true;
    }

    void set_format(formatter::Format format) {
        output_format = format;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <model_path> <input_file>\n";
        return 1;
    }

    try {
        TranscriptionApp app(argv[1]);

        if (!app.process_file(argv[2])) {
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

## Error Handling

### Error Types

```cpp
enum class LisperError {
    None,
    ModelNotFound,
    ModelLoadFailed,
    AudioNotFound,
    AudioLoadFailed,
    TranscriptionFailed,
    InsufficientMemory,
    InvalidConfiguration,
    Interrupted
};

// Exception class for critical errors
class LisperException : public std::runtime_error {
public:
    LisperException(const std::string& message, LisperError code)
        : std::runtime_error(message), error_code(code) {}

    LisperError code() const { return error_code; }

private:
    LisperError error_code;
};
```

### Error Handling Example

```cpp
try {
    Lisper engine(config);
    auto result = engine.transcribe("audio.wav");

    if (!result.success) {
        // Handle soft error
        switch (detect_error_type(result.error)) {
            case LisperError::AudioNotFound:
                std::cerr << "File not found\n";
                break;
            case LisperError::InsufficientMemory:
                std::cerr << "Out of memory - try smaller model\n";
                break;
            default:
                std::cerr << "Error: " << result.error << "\n";
        }
    }
} catch (const LisperException& e) {
    // Handle critical error
    std::cerr << "Critical error: " << e.what() << "\n";
    return 1;
}
```

## Threading and Safety

### Thread Safety

- **Lisper instances are NOT thread-safe**: Create one instance per thread
- **Model loading is thread-safe**: Multiple instances can share model file
- **Results are independent**: Each transcription gets its own result object

### Multi-threaded Example

```cpp
#include <thread>
#include <mutex>
#include <queue>

class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::string> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

    void worker_thread(const LisperConfig& config) {
        Lisper engine(config);  // One engine per thread

        while (true) {
            std::string task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] {
                    return stop || !tasks.empty();
                });

                if (stop && tasks.empty()) break;

                task = tasks.front();
                tasks.pop();
            }

            // Process task
            auto result = engine.transcribe(task);
            handle_result(task, result);
        }
    }

public:
    ThreadPool(size_t num_threads, const LisperConfig& config) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool::worker_thread,
                               this, config);
        }
    }

    void add_task(const std::string& file) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            tasks.push(file);
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& worker : workers) {
            worker.join();
        }
    }
};
```

## Performance Optimization

### Memory Management

```cpp
// Pre-allocate for batch processing
class BatchOptimized {
    Lisper engine;
    std::vector<float> audio_buffer;

public:
    BatchOptimized(const LisperConfig& config)
        : engine(config) {
        // Pre-allocate for max expected audio size
        audio_buffer.reserve(16000 * 60 * 30);  // 30 minutes
    }

    void process_batch(const std::vector<std::string>& files) {
        for (const auto& file : files) {
            // Reuse buffer
            audio_buffer.clear();
            load_audio_to_buffer(file, audio_buffer);

            auto result = engine.transcribe_raw(
                audio_buffer.data(),
                audio_buffer.size()
            );

            // Process result...
        }
    }
};
```

### GPU Acceleration

```cpp
// Configure for GPU if available
LisperConfig create_gpu_config() {
    LisperConfig config;
    config.device = LisperConfig::Device::GPU;
    config.gpu_device = 0;  // First GPU
    config.flash_attn = true;
    return config;
}

// Check GPU availability
bool is_gpu_available() {
    // Platform-specific GPU detection
    #ifdef WHISPER_CUBLAS
        return cuda_device_count() > 0;
    #elif WHISPER_METAL
        return metal_available();
    #else
        return false;
    #endif
}
```

## Integration Examples

### Qt Application

```cpp
#include <QObject>
#include <lisper/lisper.h>

class TranscriptionWorker : public QObject {
    Q_OBJECT

private:
    Lisper engine;

public:
    TranscriptionWorker(const LisperConfig& config)
        : engine(config) {}

public slots:
    void transcribe(const QString& path) {
        auto result = engine.transcribe(path.toStdString());

        if (result.success) {
            emit finished(QString::fromStdString(result.text));
        } else {
            emit error(QString::fromStdString(result.error));
        }
    }

signals:
    void finished(const QString& text);
    void error(const QString& message);
    void progress(int percent);
};
```

### Python Binding (using pybind11)

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <lisper/lisper.h>

namespace py = pybind11;

PYBIND11_MODULE(pylisper, m) {
    py::class_<LisperConfig>(m, "Config")
        .def(py::init<>())
        .def_readwrite("model_path", &LisperConfig::model_path)
        .def_readwrite("language", &LisperConfig::language)
        .def_readwrite("threads", &LisperConfig::threads);

    py::class_<TranscriptionResult>(m, "Result")
        .def_readonly("success", &TranscriptionResult::success)
        .def_readonly("text", &TranscriptionResult::text)
        .def_readonly("error", &TranscriptionResult::error);

    py::class_<Lisper>(m, "Lisper")
        .def(py::init<const LisperConfig&>())
        .def("transcribe", &Lisper::transcribe)
        .def("is_loaded", &Lisper::is_loaded);
}
```

## Best Practices

1. **Model Loading**: Load model once, reuse for multiple transcriptions
2. **Error Handling**: Always check result.success before using result.text
3. **Memory**: Use smaller/quantized models for memory-constrained systems
4. **Threading**: One Lisper instance per thread, not shared
5. **Cleanup**: Delete temporary files after processing
6. **Configuration**: Tune threads based on CPU cores and workload