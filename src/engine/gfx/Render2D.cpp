#include "gfx/Render2D.h"
#include "core/Factory.h"
#include "gfx/RenderCommand.h"
#include "gfx/Camera2D.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstring>

// ==================== Static storage ====================

Ref<IVertexArray> Render2D::s_VAO;
Ref<IBuffer>      Render2D::s_VBO;
Ref<IBuffer>      Render2D::s_IBO;
Ref<IShader>      Render2D::s_Shader;
Ref<ITexture2D>   Render2D::s_WhiteTexture;

std::vector<Render2D::QuadVertex> Render2D::s_CPUBuffer;
uint32_t Render2D::s_QuadCount = 0;

Ref<ITexture2D> Render2D::s_TextureSlots[Render2D::MaxTexSlots] = {};
uint32_t        Render2D::s_TextureSlotCount = 0;

glm::mat4 Render2D::s_ViewProj(1.0f);

Render2D::Statistics Render2D::s_Stats{};
bool Render2D::s_Initialized = false;

// ==================== Public API ====================

bool Render2D::Initialize() {
  if (s_Initialized) return true;

  s_VAO    = Factory::CreateVertexArray();
  s_VBO    = Factory::CreateBuffer(BufferType::Vertex);
  s_IBO    = Factory::CreateBuffer(BufferType::Index);
  #ifdef __EMSCRIPTEN__
  s_Shader = Factory::CreateShader("Data/Shaders/basicEs.glsl");
  #else
  s_Shader = Factory::CreateShader("Data/Shaders/basic.glsl");
  #endif

  // std::string log;
  // if (!s_Shader->CompileFromSource(kVS, kFS, &log)) {
  //   std::fprintf(stderr, "[Render2D] Shader compile/link failed: %s\n", log.c_str());
  //   return false;
  // }
  s_Shader->Bind();
  int samplers[MaxTexSlots]; for (int i=0; i<(int)MaxTexSlots; ++i) samplers[i] = i;
  s_Shader->SetIntArray("uTextures", samplers, (int)MaxTexSlots);

  // CPU staging
  s_CPUBuffer.clear();
  s_CPUBuffer.resize(MaxVertices);

  // GPU buffers
  s_VBO->SetData(nullptr, sizeof(QuadVertex) * MaxVertices, /*dynamic*/true);

  // Build index buffer once
  std::vector<uint32_t> indices(MaxIndices);
  uint32_t offset = 0;
  for (uint32_t i = 0; i < MaxIndices; i += 6) {
    indices[i + 0] = offset + 0;
    indices[i + 1] = offset + 1;
    indices[i + 2] = offset + 2;
    indices[i + 3] = offset + 0;
    indices[i + 4] = offset + 2;
    indices[i + 5] = offset + 3;
    offset += 4;
  }
  s_IBO->SetData(indices.data(), indices.size() * sizeof(uint32_t), /*dynamic*/false);

  // Vertex layout (Hazel order)
  s_VAO->Bind();
  s_VBO->Bind();
  s_IBO->Bind(); // bind EBO while VAO bound (critical)
  // aPos (vec3)
  s_VAO->EnableAttrib(0, 3, 0x1406/*GL_FLOAT*/, false, sizeof(QuadVertex), offsetof(QuadVertex, Position));
  // aColor (vec4)
  s_VAO->EnableAttrib(1, 4, 0x1406/*GL_FLOAT*/, false, sizeof(QuadVertex), offsetof(QuadVertex, Color));
  // aUV (vec2)
  s_VAO->EnableAttrib(2, 2, 0x1406/*GL_FLOAT*/, false, sizeof(QuadVertex), offsetof(QuadVertex, TexCoord));
  // aTexIndex (float)
  s_VAO->EnableAttrib(3, 1, 0x1406/*GL_FLOAT*/, false, sizeof(QuadVertex), offsetof(QuadVertex, TexIndex));
  // aTiling (float)
  s_VAO->EnableAttrib(4, 1, 0x1406/*GL_FLOAT*/, false, sizeof(QuadVertex), offsetof(QuadVertex, Tiling));
  s_VAO->Unbind();

  // 1x1 white texture in slot 0
  s_WhiteTexture = Factory::CreateTexture2D();
  const uint32_t white = 0xFFFFFFFFu;
  s_WhiteTexture->Create(1, 1, 4, &white);

  ResetStats();
  StartBatch();

  s_Initialized = true;
  return true;
}

void Render2D::Shutdown() {
  Flush(); // draw what’s left
  for (auto& textureSlot : s_TextureSlots) {
    if (textureSlot) {
      textureSlot.reset();
    }
  }
  s_TextureSlotCount = 0;
  s_Shader.reset();
  s_VAO.reset();
  s_VBO.reset();
  s_IBO.reset();
  s_WhiteTexture.reset();
  s_CPUBuffer.clear();
  s_QuadCount = 0;
  s_Initialized = false;
}

void Render2D::BeginScene(const Camera2D& cam) { BeginScene(cam.ViewProj()); }

void Render2D::BeginScene(const glm::mat4 &viewProj) {
  s_ViewProj = viewProj;
  s_Shader->Bind();
  s_Shader->SetMat4("uViewProj", s_ViewProj);
  StartBatch();
}

void Render2D::EndScene() { Flush(); }

void Render2D::Flush() {
  if (s_QuadCount == 0) return;

  const uint32_t vertCount  = s_QuadCount * 4;
  const uint32_t indexCount = s_QuadCount * 6;

  // Upload vertices
  s_VBO->UpdateSubData(0, s_CPUBuffer.data(), sizeof(QuadVertex) * vertCount);

  // Bind textures used this batch
  for (uint32_t i = 0; i < s_TextureSlotCount; ++i) {
    s_TextureSlots[i]->Bind((int)i);
    s_Stats.TextureBinds++;
  }

  // Draw
  s_VAO->Bind();     // ensure VAO (with attached EBO/attribs) is current
  s_Shader->Bind();
  s_Shader->SetMat4("uViewProj", s_ViewProj);
  RenderCommand::DrawIndexed(*s_VAO, (int)indexCount);

  s_Stats.DrawCalls++;
  StartBatch();
}

void Render2D::ResetStats() { s_Stats = {}; }
Render2D::Statistics Render2D::GetStats() { return s_Stats; }

// ==================== Draw API ====================

void Render2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
  DrawQuad(glm::vec3(pos, 0.0f), size, color);
}

void Render2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size, const glm::vec4& color) {
  glm::mat4 transform =
    glm::translate(glm::mat4(1.0f), pos) *
    glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));
  DrawQuad(transform, color);
}

void Render2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Ref<ITexture2D>& tex,
                        float tilingFactor, const glm::vec4& tint) {
  DrawQuad(glm::vec3(pos, 0.0f), size, tex, tilingFactor, tint);
}

void Render2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size, const Ref<ITexture2D>& tex,
                        float tilingFactor, const glm::vec4& tint) {
  glm::mat4 transform =
    glm::translate(glm::mat4(1.0f), pos) *
    glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));
  DrawQuad(transform, tex, tilingFactor, tint);
}

void Render2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color) {
  if (s_QuadCount >= MaxQuads) NextBatch();
  PushQuad(transform, color, /*texIndex*/ 0.0f, /*tiling*/ 1.0f);
}

void Render2D::DrawQuad(const glm::mat4& transform, const Ref<ITexture2D>& tex,
                        float tilingFactor, const glm::vec4& tint) {
  if (s_QuadCount >= MaxQuads) NextBatch();
  float texIndex = GetTextureIndexOrAppend(tex ? tex : s_WhiteTexture);
  PushQuad(transform, tint, texIndex, tilingFactor);
}

// ==================== Internals ====================

void Render2D::StartBatch() {
  s_QuadCount = 0;
  s_TextureSlotCount = 0;
  // std::memset(s_TextureSlots, 0, sizeof(s_TextureSlots));
  std::fill(std::begin(s_TextureSlots), std::end(s_TextureSlots), Ref<ITexture2D>());
  s_TextureSlots[s_TextureSlotCount++] = s_WhiteTexture; // slot 0
}

void Render2D::NextBatch() {
  Flush();
}

float Render2D::GetTextureIndexOrAppend(const Ref<ITexture2D>& texture) {
  // Look for existing
  for (uint32_t i = 0; i < s_TextureSlotCount; ++i) {
    if (s_TextureSlots[i] == texture) return (float)i;
  }
  // Need a new slot
  if (s_TextureSlotCount >= MaxTexSlots) {
    Flush();
  }
  s_TextureSlots[s_TextureSlotCount] = texture;
  return (float)(s_TextureSlotCount++);
}

void Render2D::PushQuad(const glm::mat4& transform,
                        const glm::vec4& color,
                        float texIndex,
                        float tiling) {
  // Quad in local space (Hazel canonical order)
  static const glm::vec4 kPositions[4] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 0.0f, 1.0f },
  };
  static const glm::vec2 kUV[4] = {
    { 0.0f, 0.0f },
    { 0.0f, 1.0f },
    { 1.0f, 1.0f },
    { 1.0f, 0.0f },
  };

  const uint32_t base = s_QuadCount * 4;
  if (base + 3 >= s_CPUBuffer.size()) {
    // extremely defensive; should not happen with correct MaxVerts
    Flush();
  }

  QuadVertex* v = s_CPUBuffer.data() + base;
  for (int i = 0; i < 4; ++i) {
    glm::vec4 p = transform * kPositions[i];
    v[i].Position = glm::vec3(p.x, p.y, p.z);
    v[i].Color    = color;
    v[i].TexCoord = kUV[i];
    v[i].TexIndex = texIndex;
    v[i].Tiling   = tiling;
  }

  s_QuadCount++;
  s_Stats.QuadCount++;
}

bool Rect::Contains(float px, float py) const {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

glm::vec2 Rect::Center() const {
  return {x + w * 0.5f, y + h * 0.5f};
}


const Glyph &glyphFor(char c) {
  static const std::unordered_map<char, Glyph> font = {
    {'0', {3, {"###", "# #", "# #", "# #", "###"}}},
    {'1', {3, {"  #", " ##", "  #", "  #", "  #"}}},
    {'2', {3, {"###", "  #", "###", "#  ", "###"}}},
    {'3', {3, {"###", "  #", " ##", "  #", "###"}}},
    {'4', {3, {"# #", "# #", "###", "  #", "  #"}}},
    {'5', {3, {"###", "#  ", "###", "  #", "###"}}},
    {'6', {3, {"###", "#  ", "###", "# #", "###"}}},
    {'7', {3, {"###", "  #", "  #", "  #", "  #"}}},
    {'8', {3, {"###", "# #", "###", "# #", "###"}}},
    {'9', {3, {"###", "# #", "###", "  #", "###"}}},
    {'A', {3, {"###", "# #", "###", "# #", "# #"}}},
    {'B', {3, {"## ", "# #", "## ", "# #", "## "}}},
    {'C', {4, {" ###", "#   ", "#   ", "#   ", " ###"}}},
    {'D', {3, {"## ", "# #", "# #", "# #", "## "}}},
    {'E', {3, {"###", "#  ", "###", "#  ", "###"}}},
    {'F', {3, {"###", "#  ", "###", "#  ", "#  "}}},
    {'G', {4, {" ###", "#   ", "# ##", "#  #", " ###"}}},
    {'H', {3, {"# #", "# #", "###", "# #", "# #"}}},
    {'I', {3, {"###", " # ", " # ", " # ", "###"}}},
    {'J', {3, {"###", "  #", "  #", "# #", "###"}}},
    {'K', {3, {"# #", "# #", "## ", "# #", "# #"}}},
    {'L', {3, {"#  ", "#  ", "#  ", "#  ", "###"}}},
    {'M', {3, {"# #", "###", "# #", "# #", "# #"}}},
    {'N', {4, {"# #", "## #", "# ##", "#  #", "#  #"}}},
    {'O', {3, {"###", "# #", "# #", "# #", "###"}}},
    {'P', {3, {"###", "# #", "###", "#  ", "#  "}}},
    {'Q', {4, {" ## ", "#  #", "#  #", "# ##", " ###"}}},
    {'R', {3, {"###", "# #", "###", "## ", "# #"}}},
    {'S', {4, {" ###", "#   ", " ###", "   #", "### "}}},
    {'T', {3, {"###", " # ", " # ", " # ", " # "}}},
    {'U', {3, {"# #", "# #", "# #", "# #", "###"}}},
    {'V', {3, {"# #", "# #", "# #", "# #", " # "}}},
    {'W', {3, {"# #", "# #", "# #", "###", "# #"}}},
    {'X', {3, {"# #", "# #", " # ", "# #", "# #"}}},
    {'Y', {3, {"# #", "# #", " # ", " # ", " # "}}},
    {' ', {2, {"  ", "  ", "  ", "  ", "  "}}},
    {'-', {3, {"   ", "   ", "###", "   ", "   "}}},
    {'+', {3, {"   ", " # ", "###", " # ", "   "}}},
    {'/', {4, {"   #", "  # ", "  # ", " #  ", "#   "}}},
    {'|', {3, {" # ", " # ", " # ", " # ", " # "}}},
    {':', {1, {" ", "#", " ", "#", " "}}},
    {'?', {3, {"###", "  #", " ##", "   ", " # "}}},
    {'%', {4, {"    ", "#  #", "  # ", " #  ", "#  #"}}},
  };

  auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  auto it = font.find(upper);
  if (it != font.end()) return it->second;
  return font.at('?');
}

glm::vec2 measureText(const std::string &text, float scale) {
  float maxWidth = 0.f;
  float currentWidth = 0.f;
  int lineCount = 1;
  bool hasGlyph = false;
  for (char ch : text) {
    if (ch == '\n') {
      maxWidth = std::max(maxWidth, currentWidth);
      currentWidth = 0.f;
      ++lineCount;
      continue;
    }
    const Glyph &g = glyphFor(ch);
    currentWidth += (float)g.width * scale + scale * 0.5f;
    hasGlyph = true;
  }
  maxWidth = std::max(maxWidth, currentWidth);
  if (hasGlyph && maxWidth > 0.f) {
    maxWidth -= scale * 0.5f;
  }

  float height = 0.f;
  if (hasGlyph) {
    height = scale * 5.f + static_cast<float>(lineCount - 1) * scale * 6.f;
  }

  return {maxWidth, height};
}
