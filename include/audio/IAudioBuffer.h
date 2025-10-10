#pragma once

#include "core/Types.h"

class IAudioBuffer {
public:
  virtual ~IAudioBuffer() = default;

// Interleaved PCM (16-bit or 32f). channels: 1 or 2. sampleRate: e.g., 44100.
  virtual bool LoadPCM(const void *data, size_t bytes, int channels,
                       int sampleRate, bool isFloat32) = 0;
  virtual bool  LoadWavFile(const std::string& path) = 0;

  // Optional: implement loaders for WAV/OGG in backend if you want.
  virtual float   GetDurationSec() const = 0;
  virtual int     GetChannels()    const = 0;
  virtual int     GetSampleRate()  const = 0;
  virtual bool    IsValid()        const = 0;  
};
