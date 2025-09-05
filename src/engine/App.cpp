#include "app/App.h"
#include <cstdio>
#include <chrono>
#include <thread>

App::App(std::unique_ptr<IWindow> window)
  :m_Window(std::move(window))
{
  auto &bus = m_Window->Events();

  bus.Subscribe<EWindowResize>([](const EWindowResize &e) {
     std::printf("[Event] Resize fb=%dx%d dpr=%.2f\n", e.fbWidth, e.fbHeight, e.dpr);
  });

  bus.Subscribe<EContentScale>([](const EContentScale &e) {
    std::printf("[Event] DPR changed to %.2f\n", e.dpr);
  });

  bus.Subscribe<EKey>([](const EKey& e) {
    std::printf("[Event] Key %d %s\n", (int)e.key, e.repeat ? "(repeat)" : "");
  });
}


int App::Run(){

    while (!m_Window->ShouldClose()) {
        // Update…
        // Render… (when a graphics device is attached)

        // For now, just idle a bit to avoid burning CPU.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // Swap (no-op for non-GL)
        m_Window->SwapBuffers();
        m_Window->PollEvents();
    }

    return 0;
}
