#pragma once
#include "core/FactoryDesc.h"
class IWindow;
// class GraphicsAPI;

class IGraphicsDevice{
public:
  virtual ~IGraphicsDevice() = default;
  virtual GraphicsAPI API() const = 0;
  virtual bool Initialize(IWindow& window) = 0;
  virtual void BeginFrame(int fbWidth, int fbHeight) = 0;
  virtual void EndFrame() = 0;  
};
