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
  static void PlayEx(const Ref<IAudioBuffer>& buffer, const Ref<IAudioSource>& source, bool loop = false, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);

  // static void Play

  // Access to the device if needed
  static Scope<IAudioDevice>& GetDevice();
};
