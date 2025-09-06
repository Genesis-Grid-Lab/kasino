#include "gfx/Camera2D.h"
#include <glm/gtc/matrix_transform.hpp>

Camera2D::Camera2D(){
    updateProjection(); updateView(); Update();
}
void Camera2D::SetLogicalSize(float w, float h){ m_lw=w; m_lh=h; updateProjection(); Update(); }
void Camera2D::SetPosition(float x, float y){ m_posX=x; m_posY=y; updateView(); Update(); }
void Camera2D::SetZoom(float z){ if(z<=0.0001f) z=0.0001f; m_zoom=z; updateProjection(); Update(); }
void Camera2D::SetFlipY(bool flip){ m_flipY=flip; updateProjection(); Update(); }
void Camera2D::Update(){ m_viewProj = m_proj * m_view; }

void Camera2D::updateProjection(){
    if (!m_flipY) m_proj = glm::ortho(0.f, m_lw, m_lh, 0.f, -1.f, 1.f);
    else          m_proj = glm::ortho(0.f, m_lw, 0.f,  m_lh, -1.f, 1.f);
    m_proj = glm::scale(m_proj, glm::vec3(m_zoom, m_zoom, 1.f));
}
void Camera2D::updateView(){
    m_view = glm::translate(glm::mat4(1.f), glm::vec3(-m_posX, -m_posY, 0.f));
}
