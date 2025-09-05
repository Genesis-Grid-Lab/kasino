#include "gfx/glad/GLDevice.h"
#include "window/IWindow.h"

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

void GLDevice::BeginFrame() {
    if (!m_Initialized) return;

    auto [fbw, fbh] = m_Window->GetFramebufferSize();
    glViewport(0, 0, fbw, fbh);
    glClearColor(m_Clear.x , m_Clear.y, m_Clear.z, m_Clear.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLDevice::EndFrame() {
    if (!m_Initialized) return;
    m_Window->SwapBuffers(); // GLFW path swaps; others may be no-op or custom
}
