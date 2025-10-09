#pragma once

#include "core/Types.h"
#include "IAudioBuffer.h"
#include "IAudioDevice.h"
#include "IAudioSource.h"

namespace SoundSystem {

  // Call once at app startup/shutdown
  bool Init(Scope<IAudioDevice>& device);
  void Shutdown();

  // Per-frame (optional; some backends need no ticking)
  void Update();

  // Convenience: quick one-shot sound (creates a temporary source)
  void PlayOneShot(const Ref<IAudioBuffer>& buffer, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f);

  // Access to the device if needed
  Scope<IAudioDevice>& GetDevice();
}
