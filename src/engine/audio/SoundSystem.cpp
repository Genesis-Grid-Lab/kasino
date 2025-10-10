#include "audio/SoundSystem.h"
#include "core/Log.h"
// #include <algorithm>

namespace {
    Scope<IAudioDevice> g_device;
    struct OneShot {
        Ref<IAudioSource> src;
    };
    std::vector<OneShot> g_active;
}

bool SoundSystem::Init(Scope<IAudioDevice>& device) {
    EN_CORE_INFO("Init Audio Device");
    g_device = std::move(device);
    if (!g_device) return false;
    if (!g_device->Initialize()) { g_device.reset(); return false; }
    return true;
}

void SoundSystem::Shutdown() {
    g_active.clear();
    if (g_device) { g_device->Shutdown(); g_device.reset(); }
}

void SoundSystem::Update() {
    if (!g_device) return;
    g_device->Update();
    // prune finished one-shots
    g_active.erase(std::remove_if(g_active.begin(), g_active.end(),
        [](const OneShot& o){ return !o.src || !o.src->IsPlaying(); }), g_active.end());
}

void SoundSystem::Play(const Ref<IAudioBuffer>& buffer,bool loop, float volume, float pitch, float pan) {
    if (!g_device || !buffer || !buffer->IsValid()) return;
    auto src = g_device->CreateSource();
    if (!src) return;
    src->SetBuffer(buffer);
    src->SetLooping(loop);
    src->SetVolume(volume);
    src->SetPitch(pitch);
    src->SetPan(pan);
    src->Play();
    g_active.push_back({src});
}

void SoundSystem::PlayEx(const Ref<IAudioBuffer>& buffer, const Ref<IAudioSource>& source,bool loop, float volume, float pitch, float pan) {
    if (!g_device || !buffer || !buffer->IsValid()) return;    
    if (!source) return;
    if(!source->IsPlaying()){
        source->SetBuffer(buffer);
        source->SetLooping(loop);
        source->SetVolume(volume);
        source->SetPitch(pitch);
        source->SetPan(pan);
        source->Play();
        g_active.push_back({source});
    }
}

Scope<IAudioDevice>& SoundSystem::GetDevice() { return g_device; }
