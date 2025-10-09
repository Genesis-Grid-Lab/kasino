#pragma once
#include "audio/IAudioDevice.h"
#include "audio/IAudioBuffer.h"
#include "audio/IAudioSource.h"

class NullAudioBuffer : public IAudioBuffer {
public:
    bool LoadPCM(const void*, size_t, int, int, bool) override { return true; }
    float GetDurationSec() const override { return 0.f; }
    int GetChannels() const override { return 2; }
    int GetSampleRate() const override { return 44100; }
    bool IsValid() const override { return true; }
};

class NullAudioSource : public IAudioSource {
    bool m_playing=false;
public:
    void SetBuffer(Ref<IAudioBuffer>) override {}
    void SetLooping(bool) override {}
    void SetVolume(float) override {}
    void SetPitch(float) override {}
    void SetPan(float) override {}
    void Play() override { m_playing=true; }
    void Pause() override { m_playing=false; }
    void Stop() override { m_playing=false; }
    bool IsPlaying() const override { return m_playing; }
};

class NullAudioDevice : public IAudioDevice {
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}
    void Update() override {}
    void SetMasterVolume(float) override {}
    AudioCaps GetCaps() const override { return {}; }

    Ref<IAudioBuffer> CreateBuffer() override { return CreateRef<NullAudioBuffer>(); }
    Ref<IAudioSource> CreateSource() override { return CreateRef<NullAudioSource>(); }
};
