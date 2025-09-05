#include "app/App.h"
#include <cstdio>
#include <chrono>
#include <thread>

App::App(Scope<IWindow> window, Scope<IGraphicsDevice> device)
  :m_Window(std::move(window)), m_Device(std::move(device))
{
  auto &bus = m_Window->Events();
  m_Input = CreateScope<InputSystem>(bus);
  
  bus.Subscribe<EWindowResize>([](const EWindowResize &e) {
     std::printf("[Event] Resize fb=%dx%d dpr=%.2f\n", e.fbWidth, e.fbHeight, e.dpr);
  });

  bus.Subscribe<EContentScale>([](const EContentScale &e) {
    std::printf("[Event] DPR changed to %.2f\n", e.dpr);
  });

  bus.Subscribe<EKey>([](const EKey &e) {
    std::printf("[Event] Key %d %s\n", (int)e.key, e.repeat ? "(repeat)" : "");
  });

  if (m_Device) {
    if(!m_Device->Initialize(*m_Window)){
      std::puts("[APP] Device init failed");
      m_Device.reset();
    }
  }
}


int App::Run(){

  while (!m_Window->ShouldClose()) {
    // Update…
    m_Input->BeginFrame();
    m_Window->PollEvents();

    if (m_Input->WasKeyPressed(Key::Escape)) {
      std::puts("[Input] Escape pressed (edge)");
    }
    if (m_Input->WasMousePressed(MouseButton::Left)) {
      std::printf("[Input] Mouse L pressed at (%.1f, %.1f)\n", m_Input->MouseX(), m_Input->MouseY());
    }

    if (m_Device) {
      m_Device->BeginFrame();
      //TODO: draw
      m_Device->EndFrame();
    } else {
      m_Window->SwapBuffers();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  return 0;
}
