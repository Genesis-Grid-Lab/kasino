#pragma once

#include "core/Types.h"
#include "IAudioBuffer.h"
#include "IAudioDevice.h"
#include "IAudioSource.h"

class SoundSystem {
public:
  // Call once at app startup/shutdown
  static bool Init(Scope<IAudioDevice>& device);
  static void Shutdown();

  // Per-frame (optional; some backends need no ticking)
  static void Update();

  // Convenience: quick one-shot sound (creates a temporary source)
  static void Play(const Ref<IAudioBuffer>& buffer, bool loop = false, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);
  static void Play(const Ref<IAudioBuffer>& buffer, const Ref<IAudioSource>& source, bool loop = false, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);

  static void Stop(const Ref<IAudioSource>& source);
  static void Stop();

  static void Pause(const Ref<IAudioSource>& source);
  static void Pause();

  static void Resume(const Ref<IAudioSource>& source);
  static void Resume();

  static void StopAll();

  static void SetBuffer(const Ref<IAudioSource>& source, const Ref<IAudioBuffer>& buffer);
  static void SetLooping(const Ref<IAudioSource>& source, bool loop);
  static void SetVolume(const Ref<IAudioSource>& source, float volume);
  static void SetPitch(const Ref<IAudioSource>& source, float pitch);
  static void SetPan(const Ref<IAudioSource>& source, float pan);

  // Access to the device if needed
  static Scope<IAudioDevice>& GetDevice();
};
