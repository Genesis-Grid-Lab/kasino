#include "audio/miniaudio/MiniaudioDevice.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#if __has_include("vendor/miniaudio/miniaudio.h")
#    include "vendor/miniaudio/miniaudio.h"
#else
#include <vector>

using ma_uint32 = std::uint32_t;
using ma_uint64 = std::uint64_t;
using ma_int32  = std::int32_t;
using ma_bool32 = ma_int32;
using ma_result = ma_int32;

constexpr ma_result MA_SUCCESS       =  0;
constexpr ma_result MA_INVALID_ARGS  = -1;
constexpr ma_result MA_OUT_OF_MEMORY = -2;

constexpr ma_bool32 MA_FALSE = 0;
constexpr ma_bool32 MA_TRUE  = 1;

enum ma_format {
    ma_format_unknown = 0,
    ma_format_u8,
    ma_format_s16,
    ma_format_s24,
    ma_format_s32,
    ma_format_f32
};

struct ma_engine_config {
    ma_uint32 sampleRate = 48000;
    ma_uint32 channels   = 2;
    float     volume     = 1.0f;
};

struct ma_engine {
    ma_engine_config config{};
    float masterVolume = 1.0f;
};

struct ma_sound {
    ma_engine* engine      = nullptr;
    ma_format  format      = ma_format_unknown;
    ma_uint32  channels    = 0;
    ma_uint32  sampleRate  = 0;
    ma_uint64  frameCount  = 0;
    bool       looping     = false;
    float      volume      = 1.0f;
    float      pitch       = 1.0f;
    float      pan         = 0.0f;
    bool       playing     = false;
    std::vector<float>   dataF32;
    std::vector<int16_t> dataS16;
};

inline ma_engine_config ma_engine_config_init() {
    return ma_engine_config{};
}

inline ma_result ma_engine_init(const ma_engine_config* config, ma_engine* engine) {
    if (engine == nullptr) {
        return MA_INVALID_ARGS;
    }
    ma_engine_config cfg = config ? *config : ma_engine_config_init();
    engine->config = cfg;
    engine->masterVolume = cfg.volume;
    return MA_SUCCESS;
}

inline void ma_engine_uninit(ma_engine* engine) {
    if (engine) {
        engine->masterVolume = 1.0f;
    }
}

inline void ma_engine_set_volume(ma_engine* engine, float volume) {
    if (engine) {
        engine->masterVolume = volume;
    }
}

inline ma_result ma_sound_init_from_data(ma_engine* engine,
                                         ma_sound* sound,
                                         const void* data,
                                         ma_uint64 frameCount,
                                         ma_uint32 channels,
                                         ma_uint32 sampleRate,
                                         ma_format format) {
    if (engine == nullptr || sound == nullptr || data == nullptr || channels == 0 || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }

    sound->engine = engine;
    sound->format = format;
    sound->channels = channels;
    sound->sampleRate = sampleRate;
    sound->frameCount = frameCount;
    sound->looping = false;
    sound->volume = 1.0f;
    sound->pitch = 1.0f;
    sound->pan = 0.0f;
    sound->playing = false;
    sound->dataF32.clear();
    sound->dataS16.clear();

    const ma_uint64 samples = frameCount * channels;
    if (format == ma_format_f32) {
        try {
            const float* src = static_cast<const float*>(data);
            sound->dataF32.assign(src, src + samples);
        } catch (...) {
            return MA_OUT_OF_MEMORY;
        }
    } else if (format == ma_format_s16) {
        try {
            const int16_t* src = static_cast<const int16_t*>(data);
            sound->dataS16.assign(src, src + samples);
        } catch (...) {
            return MA_OUT_OF_MEMORY;
        }
    } else {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

inline void ma_sound_uninit(ma_sound* sound) {
    if (!sound) return;
    sound->dataF32.clear();
    sound->dataS16.clear();
    sound->playing = false;
    sound->engine = nullptr;
}

inline void ma_sound_start(ma_sound* sound) {
    if (!sound) return;
    if (sound->frameCount == 0) {
        sound->playing = false;
        return;
    }
    sound->playing = true;
}

inline void ma_sound_stop(ma_sound* sound) {
    if (!sound) return;
    sound->playing = false;
}

inline ma_bool32 ma_sound_is_playing(const ma_sound* sound) {
    return (sound && sound->playing) ? MA_TRUE : MA_FALSE;
}

inline void ma_sound_set_looping(ma_sound* sound, ma_bool32 looping) {
    if (!sound) return;
    sound->looping = looping == MA_TRUE;
}

inline void ma_sound_set_volume(ma_sound* sound, float volume) {
    if (!sound) return;
    sound->volume = volume;
}

inline void ma_sound_set_pitch(ma_sound* sound, float pitch) {
    if (!sound) return;
    sound->pitch = pitch;
}

inline void ma_sound_set_pan(ma_sound* sound, float pan) {
    if (!sound) return;
    sound->pan = pan;
}
#endif

namespace {
ma_format ToMiniaudioFormat(const MiniaudioBuffer& buffer) {
    return buffer.RawIsFloat32() ? ma_format_f32 : ma_format_s16;
}

ma_uint64 CalculateFrameCount(const MiniaudioBuffer& buffer) {
    const std::size_t bytes = buffer.RawSize();
    const int channels = buffer.GetChannels();
    if (bytes == 0 || channels <= 0) {
        return 0;
    }

    const std::size_t sampleSize = buffer.RawIsFloat32() ? sizeof(float) : sizeof(int16_t);
    if (sampleSize == 0) {
        return 0;
    }
    return static_cast<ma_uint64>(bytes / (sampleSize * static_cast<std::size_t>(channels)));
}
}

// ---------------- MiniaudioBuffer ----------------
MiniaudioBuffer::MiniaudioBuffer() = default;
MiniaudioBuffer::~MiniaudioBuffer() = default;

bool MiniaudioBuffer::LoadPCM(const void* data, size_t bytes, int channels, int sampleRate, bool isFloat32) {
    if (!data || bytes == 0 || channels <= 0 || sampleRate <= 0) return false;
    m_pcm.resize(bytes);
    std::memcpy(m_pcm.data(), data, bytes);
    m_channels   = channels;
    m_sampleRate = sampleRate;
    m_isFloat32  = isFloat32;
    // duration = frames / sr
    size_t frameSize = (isFloat32 ? sizeof(float) : sizeof(short)) * (size_t)channels;
    size_t frames = (frameSize ? bytes / frameSize : 0);
    m_durationSec = (frameSize ? (float)frames / (float)sampleRate : 0.f);
    m_valid = true;
    return true;
}

namespace {
struct WavFormatChunk {
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
};

bool ReadExact(std::ifstream& file, void* dst, std::size_t bytes) {
    file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
    return static_cast<std::size_t>(file.gcount()) == bytes;
}
} // namespace

bool MiniaudioBuffer::LoadWavFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    char riff[4];
    if (!ReadExact(file, riff, sizeof(riff)) || std::strncmp(riff, "RIFF", 4) != 0) {
        return false;
    }

    std::uint32_t riffSize = 0;
    if (!ReadExact(file, &riffSize, sizeof(riffSize))) {
        return false;
    }
    (void)riffSize; // unused but kept for validation parity

    char wave[4];
    if (!ReadExact(file, wave, sizeof(wave)) || std::strncmp(wave, "WAVE", 4) != 0) {
        return false;
    }

    WavFormatChunk fmt{};
    bool fmtFound = false;
    bool dataFound = false;
    std::vector<unsigned char> data;

    auto skipBytes = [&file](std::uint32_t amount) -> bool {
        file.seekg(static_cast<std::streamoff>(amount), std::ios::cur);
        return static_cast<bool>(file);
    };

    while (file && (!fmtFound || !dataFound)) {
        char chunkId[4];
        if (!ReadExact(file, chunkId, sizeof(chunkId))) {
            break;
        }

        std::uint32_t chunkSize = 0;
        if (!ReadExact(file, &chunkSize, sizeof(chunkSize))) {
            return false;
        }

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                return false;
            }

            std::uint16_t audioFormat = 0;
            std::uint16_t channels = 0;
            std::uint32_t sampleRate = 0;
            std::uint32_t byteRate = 0;
            std::uint16_t blockAlign = 0;
            std::uint16_t bitsPerSample = 0;

            if (!ReadExact(file, &audioFormat, sizeof(audioFormat)) ||
                !ReadExact(file, &channels, sizeof(channels)) ||
                !ReadExact(file, &sampleRate, sizeof(sampleRate)) ||
                !ReadExact(file, &byteRate, sizeof(byteRate)) ||
                !ReadExact(file, &blockAlign, sizeof(blockAlign)) ||
                !ReadExact(file, &bitsPerSample, sizeof(bitsPerSample))) {
                return false;
            }

            (void)byteRate;
            (void)blockAlign;

            if (chunkSize > 16 && !skipBytes(chunkSize - 16)) {
                return false;
            }

            fmt.audioFormat = audioFormat;
            fmt.channels = channels;
            fmt.sampleRate = sampleRate;
            fmt.bitsPerSample = bitsPerSample;
            fmtFound = true;

            // fmt chunks are word aligned; skip padding if present.
            if ((chunkSize & 1u) != 0u && !skipBytes(1)) {
                return false;
            }
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            data.resize(chunkSize);
            if (chunkSize > 0 && !ReadExact(file, data.data(), data.size())) {
                return false;
            }

            if ((chunkSize & 1u) != 0u && !skipBytes(1)) {
                return false;
            }

            dataFound = true;
        } else {
            if (!skipBytes(chunkSize)) {
                return false;
            }

            if ((chunkSize & 1u) != 0u && !skipBytes(1)) {
                return false;
            }
        }
    }

    if (!fmtFound || !dataFound) {
        return false;
    }

    if (fmt.channels == 0 || fmt.sampleRate == 0 || fmt.bitsPerSample == 0) {
        return false;
    }

    const std::size_t bytesPerSample = fmt.bitsPerSample / 8;
    if (bytesPerSample == 0) {
        return false;
    }

    const std::size_t frameSize = bytesPerSample * static_cast<std::size_t>(fmt.channels);
    if (frameSize == 0 || (data.size() % frameSize) != 0) {
        return false;
    }

    const bool isFloat32 = (fmt.audioFormat == 3 && fmt.bitsPerSample == 32);
    const bool isPCM16 = (fmt.audioFormat == 1 && fmt.bitsPerSample == 16);
    if (!isFloat32 && !isPCM16) {
        return false; // unsupported format
    }

    return LoadPCM(data.data(), data.size(), fmt.channels, fmt.sampleRate, isFloat32);
}

// ---------------- MiniaudioSource ----------------
namespace {
void DestroySound(ma_sound*& sound) {
    if (!sound) {
        return;
    }
    ma_sound_uninit(sound);
    delete sound;
    sound = nullptr;
}
}

MiniaudioSource::MiniaudioSource(ma_engine* eng) : m_engine(eng) {}
MiniaudioSource::~MiniaudioSource() { DestroySound(m_sound); }

void MiniaudioSource::SetBuffer(Ref<IAudioBuffer> buffer) {
    if (!m_engine) {
        return;
    }

    if (buffer == nullptr) {
        DestroySound(m_sound);
        m_bound.reset();
        return;
    }

    auto typed = std::dynamic_pointer_cast<MiniaudioBuffer>(buffer);
    if (!typed || !typed->IsValid()) {
        DestroySound(m_sound);
        m_bound.reset();
        return;
    }

    ma_uint64 frameCount = CalculateFrameCount(*typed);
    if (frameCount == 0) {
        DestroySound(m_sound);
        m_bound.reset();
        return;
    }

    ma_sound* sound = new ma_sound();
    ma_result result = ma_sound_init_from_data(
        m_engine,
        sound,
        typed->Raw(),
        frameCount,
        static_cast<ma_uint32>(typed->GetChannels()),
        static_cast<ma_uint32>(typed->GetSampleRate()),
        ToMiniaudioFormat(*typed));

    if (result != MA_SUCCESS) {
        delete sound;
        return;
    }

    DestroySound(m_sound);
    m_sound = sound;
    m_bound = buffer;

    ma_sound_set_looping(m_sound, m_loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(m_sound, m_vol);
    ma_sound_set_pitch(m_sound, m_pitch);
    ma_sound_set_pan(m_sound, m_pan);
}

void MiniaudioSource::SetLooping(bool e) {
    m_loop = e;
    if (m_sound) {
        ma_sound_set_looping(m_sound, m_loop ? MA_TRUE : MA_FALSE);
    }
}

void MiniaudioSource::SetVolume(float v) {
    m_vol = v;
    if (m_sound) {
        ma_sound_set_volume(m_sound, v);
    }
}

void MiniaudioSource::SetPitch(float p) {
    m_pitch = p;
    if (m_sound) {
        ma_sound_set_pitch(m_sound, p);
    }
}

void MiniaudioSource::SetPan(float p) {
    m_pan = p;
    if (m_sound) {
        ma_sound_set_pan(m_sound, p);
    }
}

void MiniaudioSource::Play() {
    if (m_sound) {
        ma_sound_start(m_sound);
    }
}

void MiniaudioSource::Pause() {
    if (m_sound) {
        ma_sound_stop(m_sound);
    }
}

void MiniaudioSource::Stop() {
    if (m_sound) {
        ma_sound_stop(m_sound);
    }
}

bool MiniaudioSource::IsPlaying() const {
    return m_sound && ma_sound_is_playing(m_sound) == MA_TRUE;
}

// ---------------- MiniaudioDevice ----------------
MiniaudioDevice::MiniaudioDevice() = default;
MiniaudioDevice::~MiniaudioDevice() { Shutdown(); }

bool MiniaudioDevice::Initialize() {
    if (m_engine) {
        return true;
    }

    m_engine = new ma_engine();
    if (ma_engine_init(nullptr, m_engine) != MA_SUCCESS) {
        delete m_engine;
        m_engine = nullptr;
        return false;
    }

    ma_engine_set_volume(m_engine, m_master);
    return true;
}

void MiniaudioDevice::Shutdown() {
    if (!m_engine) {
        return;
    }

    ma_engine_uninit(m_engine);
    delete m_engine;
    m_engine = nullptr;
}

void MiniaudioDevice::Update() {
    // miniaudio engine usually doesn't need per-frame updates
}

void MiniaudioDevice::SetMasterVolume(float v) {
    m_master = v;
    if (m_engine) {
        ma_engine_set_volume(m_engine, v);
    }
}

AudioCaps MiniaudioDevice::GetCaps() const {
    AudioCaps c;
    c.supportsFloat = true;
    c.maxVoices = 64;
    return c;
}

Ref<IAudioBuffer> MiniaudioDevice::CreateBuffer() { return std::make_shared<MiniaudioBuffer>(); }
Ref<IAudioSource> MiniaudioDevice::CreateSource() {
    if (!m_engine) {
        return nullptr;
    }
    return std::make_shared<MiniaudioSource>(m_engine);
}
