#pragma once

#include "core/Types.h"

class IAudioBuffer;

class IAudioSource {
public:
  virtual ~IAudioSource() = default;

  virtual void SetBuffer(Ref<IAudioBuffer> buffer) = 0;
  virtual void SetLooping(bool enable) = 0;
  virtual void SetVolume(float vol01) = 0; // 0..1
  virtual void SetPitch(float pitch) = 0;  // 1.0 = normal
  virtual void SetPan(float pan) = 0;      // -1 ... +1

  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual bool IsPlaying() = 0;
};
