#include "app/App.h"
#include <cstdio>
#include <chrono>
#include <thread>

App::App(std::unique_ptr<IWindow> window)
    :m_Window(std::move(window))
    {

        m_Window->SetResizeCallback([this](int fbW, int fbH, float dpr){
            auto [lw, lh] = m_Window->GetLogicalSize();
            std::printf("[Resize] fb=%dx%d  logical=%dx%d  dpr=%.2f\n", fbW, fbH, lw, lh, dpr);
        });

        m_Window->SetCloseCallback([this](){
            std::puts("[Close] requested");
        });

        auto [ww, wh] = m_Window->GetWindowSize();
        auto [fbw, fbh] = m_Window->GetFramebufferSize();
        auto [lw, lh] = m_Window->GetLogicalSize();

        std::printf("[Init] window=%dx%d  framebuffer=%dx%d  logical=%dx%d  dpr=%.2f\n",
                ww, wh, fbw, fbh, lw, lh, m_Window->GetDevicePixelRatio());
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