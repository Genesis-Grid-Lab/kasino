#include "audio/miniaudio/MiniaudioDevice.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "miniaudio.h"
#include <vector>

#include "core/Log.h"

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
    EN_CORE_INFO("LoadPCM done");    
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
      EN_CORE_ERROR("Eroor file: {} donst exist");
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
    // ma_result result = ma_sound_init_from_data(
    //     m_engine,
    //     sound,
    //     typed->Raw(),
    //     frameCount,
    //     static_cast<ma_uint32>(typed->GetChannels()),
    //     static_cast<ma_uint32>(typed->GetSampleRate()),
    //     ToMiniaudioFormat(*typed));

    // if (result != MA_SUCCESS) {
    //     delete sound;
    //     return;
    // }

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

    //tmp test    
    ma_engine_play_sound(m_engine, "Resources/audio_1.wav", NULL);
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
