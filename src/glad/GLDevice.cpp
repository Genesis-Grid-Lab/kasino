#include "gfx/glad/GLDevice.h"
#include "window/IWindow.h"
#include "gfx/RenderCommand.h"
#include "core/Factory.h"
#include "gfx/ViewportUtil.h"

// GLAD
#include "glad/glad.h"

GLDevice::GLDevice(const FactoryDesc& desc){
  
}

bool GLDevice::Initialize(IWindow &window){
  m_Window = &window;

  // Ask window to ensure a GL context exist & is current (GLFW backend
  // implements this).
  if(!m_Window->EnsureGLContext(/*major*/3, /*minor*/3, /*debug*/false)){
    std::fprintf(stderr, "[GLDevice] EnsureGLContext failed\n");
    return false;
  }

  // Load GL via GLAD using window's proc loader
  auto loader = m_Window->GetGLProcLoader();
  if (!loader) {
    std::fprintf(stderr, "[GLDevice] No GL proc loader\n");
    return false;
  }

  if (!gladLoadGLLoader((GLADloadproc)loader)) {
    std::fprintf(stderr, "[GLDevice] gladLoadGLLoader failed\n");
    return false;
  }

  std::printf("[OpenGL] Vendor:   %s\n", glGetString(GL_VENDOR));
  std::printf("[OpenGL] Renderer: %s\n", glGetString(GL_RENDERER));
  std::printf("[OpenGL] Version:  %s\n", glGetString(GL_VERSION));

  // Basic GL state
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  // Enable/disable vsync on the active context
  m_Window->SetSwapInterval(m_Window->IsVsyncEnabled() ? 1 : 0);

  m_Initialized = true;
  return true;
}

void GLDevice::BeginFrame(int fbWidth, int fbHeight) {
  if (!m_Initialized) return;

  // Get logical size (your IWindow already exposes it)
  auto [lw, lh] = m_Window->GetLogicalSize();
  if (lw <= 0 || lh <= 0) { lw = fbWidth; lh = fbHeight; }

  // Integer scale that fits (pixel-perfect, centered)
  const int sW = fbWidth  / lw;
  const int sH = fbHeight / lh;
  const int S  = std::max(1, std::min(sW, sH));

  const int vpW = lw * S;
  const int vpH = lh * S;
  const int vpX = (fbWidth  - vpW) / 2;
  const int vpY = (fbHeight - vpH) / 2;

  RenderCommand::SetViewport(vpX, vpY, vpW, vpH);
}

void GLDevice::EndFrame() {
    if (!m_Initialized) return;
    m_Window->SwapBuffers(); // GLFW path swaps; others may be no-op or custom
}
