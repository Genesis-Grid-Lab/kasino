#include "audio/miniaudio/MiniaudioDevice.h"
#include <cstring>
#include <vector>

// include your third-party here in the cpp only
// #define MINIAUDIO_IMPLEMENTATION
// #include "third_party/miniaudio/miniaudio.h"

// ---------------- MiniaudioBuffer ----------------
MiniaudioBuffer::MiniaudioBuffer() = default;
MiniaudioBuffer::~MiniaudioBuffer() = default;

bool MiniaudioBuffer::LoadPCM(const void* data, size_t bytes, int channels, int sampleRate, bool isFloat32) {
    if (!data || bytes==0 || channels<=0 || sampleRate<=0) return false;
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

// ---------------- MiniaudioSource ----------------
MiniaudioSource::MiniaudioSource(ma_engine* eng) : m_engine(eng) {}
MiniaudioSource::~MiniaudioSource() { Stop(); /* destroy sound if allocated */ }

void MiniaudioSource::SetBuffer(Ref<IAudioBuffer> buffer) {
    m_bound = buffer;
    // TODO: create/destroy ma_sound with data from MiniaudioBuffer, set looping/volume/pitch/pan
}
void MiniaudioSource::SetLooping(bool e){ m_loop=e; /* if m_sound: ma_sound_set_looping(...) */ }
void MiniaudioSource::SetVolume(float v){ m_vol=v;  /* if m_sound: ma_sound_set_volume(...)  */ }
void MiniaudioSource::SetPitch(float p){ m_pitch=p; /* if m_sound: ma_sound_set_pitch(...)   */ }
void MiniaudioSource::SetPan(float p){ m_pan=p;     /* if m_sound: ma_sound_set_pan(...)     */ }
void MiniaudioSource::Play(){ /* ma_sound_start(m_sound) */ }
void MiniaudioSource::Pause(){ /* ma_sound_stop(m_sound)  */ }
void MiniaudioSource::Stop(){  /* ma_sound_uninit/free    */ }
bool MiniaudioSource::IsPlaying() const { return false; /* ma_sound_is_playing(...) */ }

// ---------------- MiniaudioDevice ----------------
MiniaudioDevice::MiniaudioDevice() = default;
MiniaudioDevice::~MiniaudioDevice() { Shutdown(); }

bool MiniaudioDevice::Initialize() {
    // TODO: ma_engine_init(...)
    return true;
}
void MiniaudioDevice::Shutdown() {
    // TODO: ma_engine_uninit(...)
}
void MiniaudioDevice::Update() {
    // miniaudio engine usually doesn't need per-frame updates
}
void MiniaudioDevice::SetMasterVolume(float v) {
    m_master = v;
    // TODO: ma_engine_set_volume(...)
}
AudioCaps MiniaudioDevice::GetCaps() const {
    AudioCaps c; c.supportsFloat = true; c.maxVoices = 64; return c;
}
Ref<IAudioBuffer> MiniaudioDevice::CreateBuffer() { return std::make_shared<MiniaudioBuffer>(); }
Ref<IAudioSource> MiniaudioDevice::CreateSource() { return std::make_shared<MiniaudioSource>(m_engine); }
