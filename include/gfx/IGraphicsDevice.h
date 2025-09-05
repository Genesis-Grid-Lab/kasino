#pragma once
class IWindow;

enum class GraphicsAPI{
  None,
  OpenGL,
  Vulkan
};

class IGraphicsDevice{
public:
  virtual ~IGraphicsDevice() = default;
  virtual GraphicsAPI API() const = 0;
  virtual bool Initialize(IWindow& window) = 0;
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;  
};
