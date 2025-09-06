#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <cstdint>

#include "gfx/IVertexArray.h"
#include "gfx/IBuffer.h"
#include "gfx/IShader.h"
#include "gfx/ITexture2D.h"

template<typename T> using Ref = std::shared_ptr<T>;
class Camera2D;

class Render2D {
public:
  struct Statistics {
    uint32_t DrawCalls   = 0;
    uint32_t QuadCount   = 0;
    uint32_t TextureBinds= 0;
  };

public:
  static bool Initialize();
  static void Shutdown();

  static void BeginScene(const Camera2D& camera);
  static void BeginScene(const glm::mat4& viewProj);
  static void EndScene();
  static void Flush();                          // force a flush

  // Solid color quad (Z default 0)
  static void DrawQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
  static void DrawQuad(const glm::vec3& pos, const glm::vec2& size, const glm::vec4& color);

  // Textured quad
  static void DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Ref<ITexture2D>& tex,
		       float tilingFactor = 1.0f, const glm::vec4& tint = glm::vec4(1.0f));
  static void DrawQuad(const glm::vec3& pos, const glm::vec2& size, const Ref<ITexture2D>& tex,
		       float tilingFactor = 1.0f, const glm::vec4& tint = glm::vec4(1.0f));

  // Transform-based
  static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
  static void DrawQuad(const glm::mat4& transform, const Ref<ITexture2D>& tex,
		       float tilingFactor = 1.0f, const glm::vec4& tint = glm::vec4(1.0f));

  static void ResetStats();
  static Statistics GetStats();

private:
  struct QuadVertex {
    glm::vec3 Position;   // aPos
    glm::vec4 Color;      // aColor
    glm::vec2 TexCoord;   // aUV
    float     TexIndex;   // aTexIndex
    float     Tiling;     // aTiling
  };

  static void StartBatch();
  static void NextBatch();
  static float GetTextureIndexOrAppend(const Ref<ITexture2D>& texture);
  static void PushQuad(const glm::mat4& transform,
		       const glm::vec4& color,
		       float texIndex,
		       float tiling);

private:
  static inline const uint32_t MaxQuads     = 20000;                     // Hazel default-ish
  static inline const uint32_t MaxVertices  = MaxQuads * 4;
  static inline const uint32_t MaxIndices   = MaxQuads * 6;
  static inline const uint32_t MaxTexSlots  = 16;                        // keep 16, matches shader ladder

private:
  // GPU
  static Ref<IVertexArray> s_VAO;
  static Ref<IBuffer>      s_VBO;
  static Ref<IBuffer>      s_IBO;
  static Ref<IShader>      s_Shader;

  // White 1x1
  static Ref<ITexture2D>   s_WhiteTexture;

  // CPU staging
  static std::vector<QuadVertex> s_CPUBuffer;
  static uint32_t                s_QuadCount;

  // Texture slots
  static Ref<ITexture2D> s_TextureSlots[MaxTexSlots];
  static uint32_t        s_TextureSlotCount;

  // Scene
  static glm::mat4 s_ViewProj;

  // Stats
  static Statistics s_Stats;

  static bool s_Initialized;
};
