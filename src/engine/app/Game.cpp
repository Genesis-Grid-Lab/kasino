#include "app/Game.h"
#include "gfx/RenderCommand.h"
#include "gfx/ViewportUtil.h"
// concrete GL device for now
#include <chrono>

bool Game::Init(const FactoryDesc& desc) {
  // 1) Window from factory (uses your FactoryDesc)
  m_Window = Factory::CreateWindow(desc);
  if (!m_Window)
    return false;

  m_Window->SetSwapInterval(1);

  // 2) Graphics device (GL today)
  m_Device = Factory::CreateDevice(desc);
  if (!m_Device->Initialize(*m_Window)) return false;

  // 3) Static Render2D
  RenderCommand::Init(Factory::CreateRendererAPI());

  // 4) Camera: derive logical size from the window itself
  auto [lw, lh] = m_Window->GetLogicalSize();
  if (lw <= 0) lw = 360;
  if (lh <= 0) lh = 640;

  m_Camera.SetLogicalSize((float)lw, (float)lh);
  m_Camera.SetFlipY(false);
  m_Camera.SetZoom(1.0f);
  m_Camera.SetPosition(0.0f, 0.0f);
  m_Camera.Update();

  // 5) First framebuffer size (pixels)
  m_fbWidth = m_Window->GetFramebufferSize().first;
  m_fbHeight = m_Window->GetFramebufferSize().second;
  if (m_fbWidth <= 0)  m_fbWidth = lw;
  if (m_fbHeight <= 0) m_fbHeight = lh;

  return OnStart();
}

void Game::Run() {
  m_Running = true;

  using clock = std::chrono::high_resolution_clock;
  auto last = clock::now();

  while (m_Running && !m_Window->ShouldClose()) {
    m_Window->PollEvents();
    RenderCommand::SetClearColor(0.5, 0.3, 0.1, 1.0);
    RenderCommand::Clear();
    auto now = clock::now();
    float dt = std::chrono::duration<float>(now - last).count();
    last = now;

    OnUpdate(dt);

    m_Device->BeginFrame(m_fbWidth, m_fbHeight);

    m_Camera.Update();
    Render2D::BeginScene(m_Camera);
    OnRender();
    Render2D::EndScene();

    m_Device->EndFrame();
  }

  OnStop();
}

void Game::Shutdown() {
  // ensure GL deletions happen with a live context
  RenderCommand::Shutdown();
  m_Device.reset();
  m_Window.reset();
}
