#pragma once

#include "gfx/Render2D.h"

#include <glm/glm.hpp>

#include <string>

namespace ui {

struct TextStyle {
  float scale = 1.0f;
  glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
  float letterSpacing = 0.5f;
  float lineSpacing = 1.0f;
};

glm::vec2 MeasureText(const std::string &text, float scale);
glm::vec2 MeasureText(const std::string &text, const TextStyle &style);

void DrawText(const std::string &text, glm::vec2 pos, const TextStyle &style);
void DrawText(const std::string &text, glm::vec2 pos, float scale,
              const glm::vec4 &color);

struct ButtonStyle {
  glm::vec4 baseColor{0.18f, 0.32f, 0.38f, 1.0f};
  glm::vec4 hoveredColor{0.30f, 0.55f, 0.78f, 1.0f};
  glm::vec4 disabledColor{0.20f, 0.20f, 0.22f, 1.0f};
  TextStyle textStyle{3.0f, glm::vec4(0.95f, 0.95f, 0.95f, 1.0f)};
  glm::vec4 hoveredTextColor{textStyle.color};
  glm::vec4 disabledTextColor{0.45f, 0.45f, 0.45f, 1.0f};
  bool drawOutline = false;
  glm::vec4 outlineColor{0.03f, 0.05f, 0.06f, 1.0f};
  glm::vec2 outlineExtend{4.0f, 4.0f};
};

struct ButtonState {
  bool hovered = false;
  bool enabled = true;
};

void DrawButton(const Rect &rect, const std::string &label,
                const ButtonStyle &style, const ButtonState &state = {});

}  // namespace ui

