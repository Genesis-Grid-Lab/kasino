#include "app/Game.h"
#include "gfx/RenderCommand.h"
#include "gfx/ViewportUtil.h"
#include "core/Log.h"
#include "audio/SoundSystem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Game::Init(const FactoryDesc &desc) {
  Log::Init();
  EN_CORE_INFO("init log done.");

  Factory::SetDesc(desc);
  // 1) Window from factory (uses your FactoryDesc)
  m_Window = Factory::CreateWindow();
  if (!m_Window)
    return false;

  m_Window->SetSwapInterval(1);

  // 2) Graphics device (GL today)
  m_Device = Factory::CreateGraphicsDevice();
  if (!m_Device->Initialize(*m_Window)) return false;

  // 3) Static Render2D
  RenderCommand::Init(Factory::CreateRendererAPI());

  // 4) audio system
  m_Audio = Factory::CreateAudioDevice();
  if(!SoundSystem::Init(m_Audio)){
    EN_CORE_ERROR("failed to init audio");
  }

  // 5) Camera: derive logical size from the window itself
  auto [lw, lh] = m_Window->GetLogicalSize();
  if (lw <= 0) lw = 360;
  if (lh <= 0) lh = 640;

  m_Camera.SetLogicalSize((float)lw, (float)lh);
  m_Camera.SetFlipY(false);
  m_Camera.SetZoom(1.0f);
  m_Camera.SetPosition(0.0f, 0.0f);
  m_Camera.Update();

  // 6) First framebuffer size (pixels)
  m_fbWidth = m_Window->GetFramebufferSize().first;
  m_fbHeight = m_Window->GetFramebufferSize().second;
  if (m_fbWidth <= 0)  m_fbWidth = lw;
  if (m_fbHeight <= 0) m_fbHeight = lh;

  return OnStart();
}


#ifdef __EMSCRIPTEN__
void Game::EmscriptenMainLoop(void* userData) {
  static_cast<Game*>(userData)->runFrame();
}
#endif

void Game::Run() {
  m_Running = true;
  m_OnStopCalled = false;

  using clock = std::chrono::high_resolution_clock;
  m_LastFrameTime = clock::now();

  SoundSystem::GetDevice()->SetMasterVolume(1);

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(&Game::EmscriptenMainLoop, this, 0, true);
#else
  while (m_Running && !m_Window->ShouldClose()) {
    runFrame();
  }
  handleStop();
#endif
}

void Game::Shutdown() {
  // ensure GL deletions happen with a live context
  RenderCommand::Shutdown();
  SoundSystem::Shutdown();
  m_Device.reset();
  m_Window.reset();
}

void Game::runFrame() {
  if (!m_Running) {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    handleStop();
    return;
  }

  using clock = std::chrono::high_resolution_clock;
  auto now = clock::now();
  float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
  m_LastFrameTime = now;

  m_Window->PollEvents();
  SoundSystem::Update();

  if (!m_Running || m_Window->ShouldClose()) {
    m_Running = false;
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    handleStop();
    return;
  }

  RenderCommand::SetClearColor(0.5f, 0.3f, 0.1f, 1.0f);
  RenderCommand::Clear();

  OnUpdate(dt);

  m_Device->BeginFrame(m_fbWidth, m_fbHeight);

  m_Camera.Update();
  Render2D::BeginScene(m_Camera);
  OnRender();
  Render2D::EndScene();

  m_Device->EndFrame();
}

void Game::handleStop() {
  if (m_OnStopCalled) return;
  m_OnStopCalled = true;
  OnStop();
}
