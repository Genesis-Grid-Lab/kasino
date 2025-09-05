#include "app/App.h"
#include <cstdio>
#include <chrono>
#include <thread>

#include "core/Factory.h"
#include "gfx/IBuffer.h"
#include "gfx/IVertexArray.h"
#include "gfx/IShader.h"
#include "glad/glad.h"

struct Vtx { float x,y; float r,g,b; };

void DrawTriangleViaFactory() {
    // Make sure GL context is already created (GLDevice::Initialize did that)
    Factory::SetGraphicsAPI(GraphicsAPI::OpenGL);

    auto vao = Factory::CreateVertexArray();
    auto vbo = Factory::CreateBuffer(BufferType::Vertex);
    auto sh  = Factory::CreateShader();

    static const char* VS = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec3 aCol;
        out vec3 vCol; void main(){ vCol=aCol; gl_Position=vec4(aPos,0,1); })";
    static const char* FS = R"(#version 330 core
        in vec3 vCol; out vec4 FragColor; void main(){ FragColor=vec4(vCol,1); })";

    std::string log;
    if(!sh->CompileFromSource(VS,FS,&log)) { /* handle error: log */ }

    Vtx tri[3] = {
        { 0.0f,  0.6f, 1,0,0 },
        {-0.6f, -0.6f, 0,1,0 },
        { 0.6f, -0.6f, 0,0,1 },
    };
    vbo->SetData(tri, sizeof(tri), false);

    vao->Bind();
    vbo->Bind();
    vao->EnableAttrib(0, 2, /*GL_FLOAT*/0x1406, false, sizeof(Vtx), offsetof(Vtx,x));
    vao->EnableAttrib(1, 3, /*GL_FLOAT*/0x1406, false, sizeof(Vtx), offsetof(Vtx,r));
    sh->Bind();
    glDrawArrays(0x0004/*GL_TRIANGLES*/, 0, 3);
    sh->Unbind();
    vao->Unbind();
}

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
      
      DrawTriangleViaFactory();

      m_Device->EndFrame();
    } else {
      m_Window->SwapBuffers();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  return 0;
}
