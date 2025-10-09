#pragma once

#include "core/Types.h"

class IAudioBuffer;
class IAudioSource;

struct AudioCaps {
  bool supportsFloat = true;
  int maxVoices = 64;
};

class IAudioDevice {
public:
  virtual ~IAudioDevice() = default;

  virtual bool Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual void Update() = 0;
  virtual void SetMasterVolume(float vol01) = 0;
  virtual AudioCaps GetCaps() const = 0;

  virtual Ref<IAudioBuffer> CreateBuffer() = 0;
  virtual Ref<IAudioSource> CreateSource() = 0;
};
