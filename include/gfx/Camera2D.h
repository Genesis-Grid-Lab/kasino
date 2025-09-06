#pragma once
#include <glm/glm.hpp>

class Camera2D {
public:
    Camera2D();

    void SetLogicalSize(float w, float h);
    void SetPosition(float x, float y);
    void SetZoom(float zoom);
    void SetFlipY(bool flip);

    void Update();

    const glm::mat4& View()     const { return m_view; }
    const glm::mat4& Proj()     const { return m_proj; }
    const glm::mat4& ViewProj() const { return m_viewProj; }

    float LogicalWidth()  const { return m_lw; }
    float LogicalHeight() const { return m_lh; }
    glm::vec2 Position()  const { return {m_posX, m_posY}; }
    float Zoom()          const { return m_zoom; }

private:
    void updateProjection();
    void updateView();

private:
    float m_lw=360.f, m_lh=640.f;
    float m_posX=0.f, m_posY=0.f;
    float m_zoom=1.f;
    bool  m_flipY=false;

    glm::mat4 m_proj{1.f};
    glm::mat4 m_view{1.f};
    glm::mat4 m_viewProj{1.f};
};
