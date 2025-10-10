#pragma once
#include "audio/IAudioDevice.h"
#include "audio/IAudioBuffer.h"
#include "audio/IAudioSource.h"

// forward-declare to avoid leaking 3rd-party in headers
struct ma_engine;
struct ma_sound;

class MiniaudioBuffer : public IAudioBuffer {
public:
    MiniaudioBuffer();
    ~MiniaudioBuffer() override;

    bool LoadPCM(const void *data, size_t bytes, int channels, int sampleRate,
                 bool isFloat32) override;
  bool  LoadWavFile(const std::string& path) override;
    float GetDurationSec() const override { return m_durationSec; }
    int   GetChannels()   const override { return m_channels; }
    int   GetSampleRate() const override { return m_sampleRate; }
    bool  IsValid()       const override { return m_valid; }

    // internal:
    const void* Raw() const { return m_pcm.data(); }
    size_t      RawSize() const { return m_pcm.size(); }
    bool        RawIsFloat32() const { return m_isFloat32; }

private:
    std::vector<unsigned char> m_pcm;
    int   m_channels=0, m_sampleRate=0;
    bool  m_isFloat32=false;
    float m_durationSec=0.f;
    bool  m_valid=false;
};

class MiniaudioSource : public IAudioSource {
public:
    MiniaudioSource(ma_engine* eng);
    ~MiniaudioSource() override;

    void SetBuffer(Ref<IAudioBuffer> buffer) override;
    void SetLooping(bool enable) override;
    void SetVolume(float vol01) override;
    void SetPitch(float pitch) override;
    void SetPan(float pan) override;

    void Play() override;
    void Pause() override;
    void Stop() override;
    bool IsPlaying() const override;

private:
    ma_engine* m_engine = nullptr;
    ma_sound*  m_sound  = nullptr; // allocated when SetBuffer called
    Ref<IAudioBuffer> m_bound;
    bool m_loop=false;
    float m_vol=1.f, m_pitch=1.f, m_pan=0.f;
};

class MiniaudioDevice : public IAudioDevice {
public:
    MiniaudioDevice();
    ~MiniaudioDevice() override;

    bool Initialize() override;
    void Shutdown() override;
    void Update() override;
    void SetMasterVolume(float vol01) override;
    AudioCaps GetCaps() const override;

    Ref<IAudioBuffer> CreateBuffer() override;
    Ref<IAudioSource> CreateSource() override;

private:
    ma_engine* m_engine = nullptr; // allocated in Initialize
    float m_master=1.f;
};
