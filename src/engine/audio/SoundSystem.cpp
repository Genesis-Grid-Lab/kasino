#include "audio/SoundSystem.h"
#include "core/Log.h"
#include <algorithm>

namespace {
    Scope<IAudioDevice> g_device;
    struct OneShot {
        Ref<IAudioSource> src;
    };
    std::vector<OneShot> g_active;

    void TrackSource(const Ref<IAudioSource>& source){
        if(!source) return;
        auto it = std::find_if(g_active.begin(), g_active.end(), [&](const OneShot& other){
            return other.src == source;
        });

        if(it == g_active.end()){
            g_active.push_back({source});
        }
    }

    void UntrackSource(const Ref<IAudioSource>& source){
        if(!source) return;

        g_active.erase(std::remove_if(g_active.begin(), g_active.end(), [&](const OneShot& other){
            return !other.src || other.src == source;
        }), g_active.end());
    }
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
    TrackSource(src);
}

void SoundSystem::Play(const Ref<IAudioBuffer>& buffer, const Ref<IAudioSource>& source,bool loop, float volume, float pitch, float pan) {
    if (!g_device || !buffer || !buffer->IsValid()) return;    
    if (!source) return;
    if(!source->IsPlaying()){
        source->SetBuffer(buffer);
        source->SetLooping(loop);
        source->SetVolume(volume);
        source->SetPitch(pitch);
        source->SetPan(pan);
        source->Play();
        TrackSource(source);
    }
}

void SoundSystem::Stop(const Ref<IAudioSource>& source){
    if(!g_device || !source) return;
    source->Stop();
    UntrackSource(source);
}
void SoundSystem::Stop(){
    if(!g_device) return;

    for(auto& entry : g_active){
        if(entry.src->IsPlaying()){
            entry.src->Stop();
            UntrackSource(entry.src);
        }
    }    
}

  void SoundSystem::Pause(const Ref<IAudioSource>& source){
    if(!g_device || !source) return;
    source->Pause();
    TrackSource(source);
  }
  void SoundSystem::Pause(){

  }

  void SoundSystem::Resume(const Ref<IAudioSource>& source){
    if(!g_device || !source) return;
    source->Play();
    TrackSource(source);
  }
  void SoundSystem::Resume(){

  }

  void SoundSystem::StopAll(){
    if(!g_device) return;
    for(auto& entry : g_active){
        if(entry.src){
            entry.src->Stop();
        }
    }

    g_active.clear();
  }

  void SoundSystem::SetBuffer(const Ref<IAudioSource>& source, const Ref<IAudioBuffer>& buffer){
    if(!g_device || !source) return;
    source->SetBuffer(buffer);
    TrackSource(source);
  }
  void SoundSystem::SetLooping(const Ref<IAudioSource>& source, bool loop){
    if(!g_device || !source) return;

    source->SetLooping(loop);
    TrackSource(source);
  }
  void SoundSystem::SetVolume(const Ref<IAudioSource>& source, float volume){
    if(!g_device || !source) return;
    source->SetVolume(volume);
    TrackSource(source);
  }
  void SoundSystem::SetPitch(const Ref<IAudioSource>& source, float pitch){
    if(!g_device || !source) return;
    source->SetPitch(pitch);
    TrackSource(source);
  }
  void SoundSystem::SetPan(const Ref<IAudioSource>& source, float pan){
    if(!g_device || !source) return;
    source->SetPan(pan);
    TrackSource(source);
  }

Scope<IAudioDevice>& SoundSystem::GetDevice() { return g_device; }
