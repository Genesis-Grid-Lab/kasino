#pragma once

#include "core/Types.h"
#include "gfx/IGraphicsDevice.h"

class GLDevice : public IGraphicsDevice{
 public:
  explicit GLDevice(const FactoryDesc& desc);
  GraphicsAPI API() const override { return GraphicsAPI::OpenGL; }
  bool Initialize(IWindow& window) override;
  void BeginFrame() override;
  void EndFrame() override;

  void SetClearColor(glm::vec4 color) {
    color = m_Clear;
  }

 private:
  class IWindow* m_Window = nullptr;
  glm::vec4 m_Clear = {0.08f, 0.09f, 0.10f, 1.0f};
  bool m_Initialized = false;
};
