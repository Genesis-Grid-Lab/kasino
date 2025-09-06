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
  float w = m_lw;
  float h = m_lh;
  if (m_zoom <= 0.0001f) m_zoom = 0.0001f;
  w /= m_zoom;
  h /= m_zoom;

  if (!m_flipY) m_proj = glm::ortho(0.f, w, h, 0.f, -1.f, 1.f);
  else          m_proj = glm::ortho(0.f, w, 0.f,  h, -1.f, 1.f);
}
void Camera2D::updateView(){
  float px = std::round(m_posX);
  float py = std::round(m_posY);
  m_view = glm::translate(glm::mat4(1.f), glm::vec3(-px, -py, 0.f));
}
