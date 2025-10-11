#include "ui/UISystem.h"

#include "gfx/Render2D.h"

#include <algorithm>

namespace ui {

namespace {

float LineAdvance(const TextStyle &style) {
  return style.scale * (5.0f + style.lineSpacing);
}

float LetterSpacing(const TextStyle &style) {
  return style.scale * style.letterSpacing;
}

}  // namespace

glm::vec2 MeasureText(const std::string &text, float scale) {
  return MeasureText(text, TextStyle{scale});
}

glm::vec2 MeasureText(const std::string &text, const TextStyle &style) {
  float maxWidth = 0.0f;
  float currentWidth = 0.0f;
  int lineCount = 1;
  bool lineHasGlyph = false;
  bool hasGlyph = false;
  const float spacing = LetterSpacing(style);

  for (char ch : text) {
    if (ch == '\n') {
      maxWidth = std::max(maxWidth, currentWidth);
      currentWidth = 0.0f;
      lineHasGlyph = false;
      ++lineCount;
      continue;
    }

    const Glyph &g = glyphFor(ch);
    if (lineHasGlyph) {
      currentWidth += spacing;
    }
    currentWidth += static_cast<float>(g.width) * style.scale;
    lineHasGlyph = true;
    hasGlyph = true;
  }

  maxWidth = std::max(maxWidth, currentWidth);

  if (!hasGlyph) {
    return {0.0f, 0.0f};
  }

  float height = style.scale * 5.0f;
  if (lineCount > 1) {
    height += static_cast<float>(lineCount - 1) * LineAdvance(style);
  }

  return {maxWidth, height};
}

void DrawText(const std::string &text, glm::vec2 pos, const TextStyle &style) {
  float x = pos.x;
  float y = pos.y;
  bool lineHasGlyph = false;
  const float spacing = LetterSpacing(style);

  for (char ch : text) {
    if (ch == '\n') {
      y += LineAdvance(style);
      x = pos.x;
      lineHasGlyph = false;
      continue;
    }

    if (lineHasGlyph) {
      x += spacing;
    }

    const Glyph &g = glyphFor(ch);
    for (int row = 0; row < 5; ++row) {
      const char *rowStr = g.rows[row];
      for (int col = 0; col < g.width && rowStr[col] != '\0'; ++col) {
        if (rowStr[col] == ' ' || rowStr[col] == '\0') {
          continue;
        }
        glm::vec2 cellPos{x + static_cast<float>(col) * style.scale,
                          y + static_cast<float>(row) * style.scale};
        Render2D::DrawQuad(cellPos, glm::vec2{style.scale, style.scale},
                           style.color);
      }
    }

    x += static_cast<float>(g.width) * style.scale;
    lineHasGlyph = true;
  }
}

void DrawText(const std::string &text, glm::vec2 pos, float scale,
              const glm::vec4 &color) {
  DrawText(text, pos, TextStyle{scale, color});
}

void DrawButton(const Rect &rect, const std::string &label,
                const ButtonStyle &style, const ButtonState &state) {
  if (rect.w <= 0.0f || rect.h <= 0.0f || label.empty()) {
    return;
  }

  if (style.drawOutline) {
    glm::vec2 paddedSize{rect.w + style.outlineExtend.x * 2.0f,
                         rect.h + style.outlineExtend.y * 2.0f};
    glm::vec2 outlinePos{rect.x - style.outlineExtend.x,
                         rect.y - style.outlineExtend.y};
    Render2D::DrawQuad(outlinePos, paddedSize, style.outlineColor);
  }

  glm::vec4 fillColor = style.baseColor;
  if (!state.enabled) {
    fillColor = style.disabledColor;
  } else if (state.hovered) {
    fillColor = style.hoveredColor;
  }

  Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                     fillColor);

  glm::vec4 textColor = style.textStyle.color;
  if (!state.enabled) {
    textColor = style.disabledTextColor;
  } else if (state.hovered) {
    textColor = style.hoveredTextColor;
  }

  TextStyle textStyle = style.textStyle;
  textStyle.color = textColor;
  glm::vec2 metrics = MeasureText(label, textStyle);
  float textX = rect.x + rect.w * 0.5f - metrics.x * 0.5f;
  float textY = rect.y + rect.h * 0.5f - metrics.y * 0.5f;
  DrawText(label, glm::vec2{textX, textY}, textStyle);
}

}  // namespace ui

