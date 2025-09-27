#include "Kasino/KasinoGame.h"

#include "app/Game.h"
#include "gfx/Render2D.h"
#include "input/InputSystem.h"
#include "Kasino/GameLogic.h"
#include "Kasino/Scoring.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace {

struct Glyph {
  int width = 3;
  std::array<const char *, 5> rows{};
};

const Glyph &glyphFor(char c) {
  static const std::unordered_map<char, Glyph> font = {
      {'0', {3, {"###", "# #", "# #", "# #", "###"}}},
      {'1', {3, {" ##", "  #", "  #", "  #", " ###"}}},
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
      {'K', {3, {"# #", "# #", "## ", "# #", "# #"}}},
      {'L', {3, {"#  ", "#  ", "#  ", "#  ", "###"}}},
      {'M', {3, {"# #", "###", "# #", "# #", "# #"}}},
      {'N', {4, {"# #", "## #", "# ##", "#  #", "#  #"}}},
      {'O', {3, {"###", "# #", "# #", "# #", "###"}}},
      {'P', {3, {"###", "# #", "###", "#  ", "#  "}}},
      {'R', {3, {"###", "# #", "###", "## ", "# #"}}},
      {'S', {4, {" ###", "#   ", " ###", "   #", "### "}}},
      {'T', {3, {"###", " # ", " # ", " # ", " # "}}},
      {'U', {3, {"# #", "# #", "# #", "# #", "###"}}},
      {'V', {3, {"# #", "# #", "# #", "# #", " # "}}},
      {'W', {3, {"# #", "# #", "# #", "###", "# #"}}},
      {'Y', {3, {"# #", "# #", " # ", " # ", " # "}}},
      {' ', {2, {"  ", "  ", "  ", "  ", "  "}}},
      {'-', {3, {"   ", "   ", "###", "   ", "   "}}},
      {':', {1, {" ", "#", " ", "#", " "}}},
      {'?', {3, {"###", "  #", " ##", "   ", " # "}}},
  };

  auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  auto it = font.find(upper);
  if (it != font.end()) return it->second;
  return font.at('?');
}

}  // namespace

bool KasinoGame::Rect::Contains(float px, float py) const {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

glm::vec2 KasinoGame::Rect::Center() const {
  return {x + w * 0.5f, y + h * 0.5f};
}

void KasinoGame::Selection::Clear() {
  handIndex.reset();
  loose.clear();
  builds.clear();
}

bool KasinoGame::OnStart() {
  if (!Render2D::Initialize()) return false;

  m_Window->SetResizeCallback([this](int fbW, int fbH, float) {
    (void)fbW;
    (void)fbH;
    // Camera logical size stays constant; rendering already handles resize.
  });

  m_Input = std::make_unique<InputSystem>(m_Window->Events());

  m_State.numPlayers = 2;
  startNewMatch();
  return true;
}

void KasinoGame::OnStop() {
  m_Input.reset();
  Render2D::Shutdown();
}

void KasinoGame::startNewMatch() {
  m_TotalScores.assign(m_State.numPlayers, 0);
  m_RoundNumber = 1;
  m_WinningPlayer = -1;
  Casino::StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = Casino::LegalMoves(m_State);
  m_Selection.Clear();
  m_ActionEntries.clear();
  m_LastRoundScores.clear();
  m_ShowPrompt = false;
  refreshHighlights();
}

void KasinoGame::startNextRound() {
  ++m_RoundNumber;
  Casino::StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = Casino::LegalMoves(m_State);
  m_Selection.Clear();
  m_ActionEntries.clear();
  m_LastRoundScores.clear();
  m_Phase = Phase::Playing;
  m_ShowPrompt = false;
  refreshHighlights();
}

void KasinoGame::updateLegalMoves() {
  m_LegalMoves = Casino::LegalMoves(m_State);
  if (m_Selection.handIndex) {
    // Ensure the selected index is still valid; otherwise clear selection.
    auto &hand = m_State.players[m_State.current].hand;
    if (*m_Selection.handIndex < 0 ||
        *m_Selection.handIndex >= static_cast<int>(hand.size())) {
      m_Selection.Clear();
      m_ActionEntries.clear();
    }
  }
  refreshHighlights();
}

void KasinoGame::updateLayout() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float margin = 12.f;
  float panelWidth = 150.f;

  m_TableRect.x = margin;
  m_TableRect.y = m_ScoreboardHeight + margin * 2.f + m_CardHeight + 24.f;
  m_TableRect.w = width - panelWidth - margin * 3.f;
  float bottomLimit = height - m_CardHeight - margin * 2.f - 32.f;
  m_TableRect.h = std::max(120.f, bottomLimit - m_TableRect.y);

  m_ActionPanelRect.x = m_TableRect.x + m_TableRect.w + margin;
  m_ActionPanelRect.y = m_TableRect.y;
  m_ActionPanelRect.w = panelWidth;
  m_ActionPanelRect.h = m_TableRect.h;

  // Layout hands
  m_PlayerHandRects.assign(m_State.numPlayers, {});
  for (int p = 0; p < m_State.numPlayers; ++p) {
    const auto &hand = m_State.players[p].hand;
    auto &rects = m_PlayerHandRects[p];
    rects.clear();
    rects.reserve(hand.size());

    float spacing = m_CardWidth * 0.2f;
    float totalWidth = hand.empty()
                           ? 0.f
                           : (float)hand.size() * m_CardWidth +
                                 (float)(hand.size() - 1) * spacing;
    float startX = std::max(margin, (width - totalWidth) * 0.5f);
    float y = (p == 0) ? (height - m_CardHeight - margin)
                       : (m_ScoreboardHeight + margin);
    if (p > 1) {
      // additional players could be stacked above table; offset slightly
      y = m_TableRect.y - (float)(p) * (m_CardHeight + 16.f);
    }
    for (size_t i = 0; i < hand.size(); ++i) {
      rects.push_back(Rect{startX + (float)i * (m_CardWidth + spacing), y,
                           m_CardWidth, m_CardHeight});
    }
  }

  // Layout loose cards in grid
  m_LooseRects.clear();
  const auto &loose = m_State.table.loose;
  int columns = std::max(1, (int)(m_TableRect.w / (m_CardWidth + 10.f)));
  float looseSpacing = 10.f;
  for (size_t i = 0; i < loose.size(); ++i) {
    int row = static_cast<int>(i) / columns;
    int col = static_cast<int>(i) % columns;
    Rect r;
    r.x = m_TableRect.x + looseSpacing + col * (m_CardWidth + looseSpacing);
    r.y = m_TableRect.y + looseSpacing + row * (m_CardHeight + looseSpacing);
    r.w = m_CardWidth;
    r.h = m_CardHeight;
    m_LooseRects.push_back(r);
  }

  // Layout builds in a row along bottom of table
  m_BuildRects.clear();
  const auto &builds = m_State.table.builds;
  float buildHeight = m_CardHeight * 0.8f;
  float buildWidth = m_CardWidth * 1.1f;
  for (size_t i = 0; i < builds.size(); ++i) {
    Rect r;
    r.w = buildWidth;
    r.h = buildHeight;
    r.x = m_TableRect.x + 14.f + (float)i * (buildWidth + 12.f);
    r.y = m_TableRect.y + m_TableRect.h - buildHeight - 12.f;
    m_BuildRects.push_back(r);
  }

  m_LooseHighlights.assign(m_LooseRects.size(), false);
  m_BuildHighlights.assign(m_BuildRects.size(), false);

  layoutActionEntries();

  // Cancel button near bottom of panel
  float buttonHeight = 32.f;
  m_CancelButtonRect = {m_ActionPanelRect.x + 12.f,
                        m_ActionPanelRect.y + m_ActionPanelRect.h - buttonHeight - 12.f,
                        m_ActionPanelRect.w - 24.f, buttonHeight};

  if (m_ShowPrompt) {
    float boxWidth = width * 0.75f;
    float boxHeight = (m_Phase == Phase::RoundSummary ? 260.f : 200.f);
    m_PromptBoxRect = {width * 0.5f - boxWidth * 0.5f,
                       height * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight};
    m_PromptButtonRect = {m_PromptBoxRect.x + (boxWidth - 180.f) * 0.5f,
                          m_PromptBoxRect.y + boxHeight - 56.f, 180.f, 40.f};
  }
}

void KasinoGame::layoutActionEntries() {
  float buttonHeight = 40.f;
  float x = m_ActionPanelRect.x + 12.f;
  float y = m_ActionPanelRect.y + 12.f;
  float w = m_ActionPanelRect.w - 24.f;
  for (auto &entry : m_ActionEntries) {
    entry.rect = {x, y, w, buttonHeight};
    y += buttonHeight + 8.f;
  }
}

void KasinoGame::refreshHighlights() {
  m_LooseHighlights.assign(m_LooseRects.size(), false);
  m_BuildHighlights.assign(m_BuildRects.size(), false);

  if (!m_Selection.handIndex) return;

  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard ==
          m_State.players[m_State.current].hand[*m_Selection.handIndex]))
      continue;
    if (!selectionCompatible(mv)) continue;

    if (mv.type == Casino::MoveType::Capture) {
      for (int idx : mv.captureLooseIdx)
        if (idx >= 0 && idx < (int)m_LooseHighlights.size())
          m_LooseHighlights[idx] = true;
      for (int idx : mv.captureBuildIdx)
        if (idx >= 0 && idx < (int)m_BuildHighlights.size())
          m_BuildHighlights[idx] = true;
    } else if (mv.type == Casino::MoveType::Build) {
      for (int idx : mv.buildUseLooseIdx)
        if (idx >= 0 && idx < (int)m_LooseHighlights.size())
          m_LooseHighlights[idx] = true;
    } else if (mv.type == Casino::MoveType::ExtendBuild) {
      for (int idx : mv.captureBuildIdx)
        if (idx >= 0 && idx < (int)m_BuildHighlights.size())
          m_BuildHighlights[idx] = true;
    }
  }
}

bool KasinoGame::selectionCompatible(const Casino::Move &mv) const {
  if (!m_Selection.handIndex) return false;

  auto subset = [](const std::set<int> &selected,
                   const std::vector<int> &required) {
    for (int idx : selected) {
      if (std::find(required.begin(), required.end(), idx) ==
          required.end())
        return false;
    }
    return true;
  };

  switch (mv.type) {
  case Casino::MoveType::Capture:
    return subset(m_Selection.loose, mv.captureLooseIdx) &&
           subset(m_Selection.builds, mv.captureBuildIdx);
  case Casino::MoveType::Build:
    return m_Selection.builds.empty() &&
           subset(m_Selection.loose, mv.buildUseLooseIdx);
  case Casino::MoveType::ExtendBuild:
    return m_Selection.loose.empty() &&
           subset(m_Selection.builds, mv.captureBuildIdx);
  case Casino::MoveType::Trail:
    return m_Selection.loose.empty() && m_Selection.builds.empty();
  }
  return false;
}

std::string KasinoGame::moveLabel(const Casino::Move &mv) const {
  std::ostringstream oss;
  switch (mv.type) {
  case Casino::MoveType::Capture: {
    oss << "CAPTURE";
    for (int idx : mv.captureLooseIdx) {
      if (idx >= 0 && idx < (int)m_State.table.loose.size())
        oss << ' ' << m_State.table.loose[idx].ToString();
    }
    for (int idx : mv.captureBuildIdx) {
      oss << " B" << (idx + 1);
    }
  } break;
  case Casino::MoveType::Build: {
    oss << "BUILD TO " << mv.buildTargetValue;
    for (int idx : mv.buildUseLooseIdx) {
      if (idx >= 0 && idx < (int)m_State.table.loose.size())
        oss << ' ' << m_State.table.loose[idx].ToString();
    }
  } break;
  case Casino::MoveType::ExtendBuild: {
    oss << "RAISE TO " << mv.buildTargetValue;
    for (int idx : mv.captureBuildIdx) {
      oss << " B" << (idx + 1);
    }
  } break;
  case Casino::MoveType::Trail:
    oss << "TRAIL";
    break;
  }
  return oss.str();
}

void KasinoGame::updateHoveredAction(float mx, float my) {
  m_HoveredAction = -1;
  m_HoveredLoose.clear();
  m_HoveredBuilds.clear();
  if (m_ShowPrompt) return;

  for (size_t i = 0; i < m_ActionEntries.size(); ++i) {
    if (m_ActionEntries[i].rect.Contains(mx, my)) {
      m_HoveredAction = static_cast<int>(i);
      const auto &mv = m_ActionEntries[i].move;
      for (int idx : mv.captureLooseIdx) m_HoveredLoose.insert(idx);
      for (int idx : mv.buildUseLooseIdx) m_HoveredLoose.insert(idx);
      for (int idx : mv.captureBuildIdx) m_HoveredBuilds.insert(idx);
      break;
    }
  }
}

void KasinoGame::selectHandCard(int player, int index) {
  if (m_State.current != player) return;
  if (m_Selection.handIndex && *m_Selection.handIndex == index) {
    m_Selection.Clear();
    m_ActionEntries.clear();
    refreshHighlights();
    return;
  }

  m_Selection.handIndex = index;
  m_Selection.loose.clear();
  m_Selection.builds.clear();
  m_ActionEntries.clear();

  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard == m_State.players[player].hand[index])) continue;
    if (!selectionCompatible(mv)) continue;
    ActionEntry entry;
    entry.move = mv;
    entry.label = moveLabel(mv);
    m_ActionEntries.push_back(entry);
  }

  layoutActionEntries();
  refreshHighlights();
}

void KasinoGame::toggleLooseCard(int idx) {
  if (!m_Selection.handIndex) return;
  if (m_Selection.loose.count(idx))
    m_Selection.loose.erase(idx);
  else
    m_Selection.loose.insert(idx);

  m_ActionEntries.clear();
  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard ==
          m_State.players[m_State.current].hand[*m_Selection.handIndex]))
      continue;
    if (!selectionCompatible(mv)) continue;
    ActionEntry entry;
    entry.move = mv;
    entry.label = moveLabel(mv);
    m_ActionEntries.push_back(entry);
  }
  layoutActionEntries();
  refreshHighlights();
}

void KasinoGame::toggleBuild(int idx) {
  if (!m_Selection.handIndex) return;
  if (m_Selection.builds.count(idx))
    m_Selection.builds.erase(idx);
  else
    m_Selection.builds.insert(idx);

  m_ActionEntries.clear();
  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard ==
          m_State.players[m_State.current].hand[*m_Selection.handIndex]))
      continue;
    if (!selectionCompatible(mv)) continue;
    ActionEntry entry;
    entry.move = mv;
    entry.label = moveLabel(mv);
    m_ActionEntries.push_back(entry);
  }
  layoutActionEntries();
  refreshHighlights();
}

void KasinoGame::processInput(float mx, float my) {
  if (!m_Input) return;
  bool click = m_Input->WasMousePressed(MouseButton::Left);
  if (!click) return;

  if (m_ShowPrompt) {
    if (m_PromptButtonRect.Contains(mx, my)) {
      handlePrompt();
    }
    return;
  }

  if (m_Phase != Phase::Playing) return;

  // Action entries
  for (size_t i = 0; i < m_ActionEntries.size(); ++i) {
    if (m_ActionEntries[i].rect.Contains(mx, my)) {
      applyMove(m_ActionEntries[i].move);
      return;
    }
  }

  if (m_Selection.handIndex && m_CancelButtonRect.Contains(mx, my)) {
    m_Selection.Clear();
    m_ActionEntries.clear();
    refreshHighlights();
    return;
  }

  // Hand cards
  for (int p = 0; p < m_State.numPlayers; ++p) {
    const auto &rects = m_PlayerHandRects[p];
    for (size_t i = 0; i < rects.size(); ++i) {
      if (rects[i].Contains(mx, my)) {
        selectHandCard(p, static_cast<int>(i));
        return;
      }
    }
  }

  // Table loose cards
  for (size_t i = 0; i < m_LooseRects.size(); ++i) {
    if (m_LooseRects[i].Contains(mx, my)) {
      toggleLooseCard(static_cast<int>(i));
      return;
    }
  }

  // Builds
  for (size_t i = 0; i < m_BuildRects.size(); ++i) {
    if (m_BuildRects[i].Contains(mx, my)) {
      toggleBuild(static_cast<int>(i));
      return;
    }
  }

  // Background click clears selection
  m_Selection.Clear();
  m_ActionEntries.clear();
  refreshHighlights();
}

void KasinoGame::applyMove(const Casino::Move &mv) {
  if (!Casino::ApplyMove(m_State, mv)) return;

  m_Selection.Clear();
  m_ActionEntries.clear();
  updateLegalMoves();
  layoutActionEntries();

  if (m_State.RoundOver()) {
    handleRoundEnd();
  }
}

void KasinoGame::handleRoundEnd() {
  m_LastRoundScores = Casino::ScoreRound(m_State);
  if (m_TotalScores.size() != static_cast<size_t>(m_State.numPlayers))
    m_TotalScores.assign(m_State.numPlayers, 0);

  bool hasWinner = false;
  m_WinningPlayer = -1;
  for (int p = 0; p < m_State.numPlayers; ++p) {
    m_TotalScores[p] += m_LastRoundScores[p].total;
    if (m_TotalScores[p] >= m_TargetScore) {
      if (!hasWinner || m_TotalScores[p] > m_TotalScores[m_WinningPlayer]) {
        hasWinner = true;
        m_WinningPlayer = p;
      }
    }
  }

  m_Phase = hasWinner ? Phase::MatchSummary : Phase::RoundSummary;
  m_ShowPrompt = true;
  if (hasWinner) {
    m_PromptHeader = "PLAYER " + std::to_string(m_WinningPlayer + 1) +
                     " WINS";
    m_PromptButtonLabel = "NEW MATCH";
  } else {
    m_PromptHeader = "ROUND " + std::to_string(m_RoundNumber) + " COMPLETE";
    m_PromptButtonLabel = "NEXT ROUND";
  }
}

void KasinoGame::handlePrompt() {
  if (m_Phase == Phase::MatchSummary) {
    startNewMatch();
  } else if (m_Phase == Phase::RoundSummary) {
    startNextRound();
  }
}

void KasinoGame::OnUpdate(float dt) {
  (void)dt;
  if (!m_Input) return;

  updateLayout();
  refreshHighlights();
  float mx = m_Input->MouseX();
  float my = m_Input->MouseY();
  updateHoveredAction(mx, my);
  processInput(mx, my);

  if (m_Phase == Phase::Playing && m_State.RoundOver()) {
    handleRoundEnd();
  }

  m_Input->BeginFrame();
}

void KasinoGame::drawCardFace(const Casino::Card &card, const Rect &r,
                              bool isCurrent, bool selected, bool legal,
                              bool hovered) {
  glm::vec4 borderColor = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
  glm::vec4 faceColor = glm::vec4(1.0f);
  if (!isCurrent) {
    faceColor = glm::mix(faceColor, glm::vec4(0.8f, 0.8f, 0.8f, 1.f), 0.35f);
  }
  Render2D::DrawQuad(glm::vec2{r.x - 2.f, r.y - 2.f},
                     glm::vec2{r.w + 4.f, r.h + 4.f}, borderColor);
  Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h}, faceColor);

  if (legal)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.2f, 0.45f, 0.9f, 0.25f));
  if (hovered)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.95f, 0.55f, 0.25f, 0.4f));
  if (selected)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.95f, 0.85f, 0.2f, 0.45f));

  auto rankString = [&]() {
    switch (card.rank) {
    case Casino::Rank::Ace:
      return std::string("A");
    case Casino::Rank::Jack:
      return std::string("J");
    case Casino::Rank::Queen:
      return std::string("Q");
    case Casino::Rank::King:
      return std::string("K");
    default:
      return std::to_string(Casino::RankValue(card.rank));
    }
  }();

  auto suitString = [&]() {
    switch (card.suit) {
    case Casino::Suit::Clubs:
      return std::string("C");
    case Casino::Suit::Diamonds:
      return std::string("D");
    case Casino::Suit::Hearts:
      return std::string("H");
    case Casino::Suit::Spades:
      return std::string("S");
    }
    return std::string("?");
  }();

  glm::vec4 textColor =
      (card.suit == Casino::Suit::Hearts ||
       card.suit == Casino::Suit::Diamonds)
          ? glm::vec4(0.80f, 0.1f, 0.1f, 1.0f)
          : glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

  float scale = r.w / 10.f;
  drawText(rankString, glm::vec2{r.x + 6.f, r.y + 6.f}, scale, textColor);
  drawText(suitString,
           glm::vec2{r.x + 6.f, r.y + 6.f + 6.f * scale}, scale, textColor);
}

void KasinoGame::drawBuildFace(const Casino::Build &build, const Rect &r,
                               bool legal, bool hovered, bool selected) {
  int ownerIndex = build.ownerPlayer >= 0 ? build.ownerPlayer : 0;
  glm::vec4 ownerColor =
      m_PlayerColors[ownerIndex % m_PlayerColors.size()];
  glm::vec4 base = glm::mix(ownerColor, glm::vec4(0.1f, 0.2f, 0.15f, 1.0f), 0.6f);
  Render2D::DrawQuad(glm::vec2{r.x - 2.f, r.y - 2.f},
                     glm::vec2{r.w + 4.f, r.h + 4.f},
                     glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h}, base);
  if (legal)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.3f, 0.6f, 0.95f, 0.25f));
  if (hovered)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.95f, 0.5f, 0.2f, 0.35f));
  if (selected)
    Render2D::DrawQuad(glm::vec2{r.x, r.y}, glm::vec2{r.w, r.h},
                       glm::vec4(0.95f, 0.85f, 0.2f, 0.4f));

  drawText("BUILD", glm::vec2{r.x + 6.f, r.y + 6.f}, r.w / 14.f,
           glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  drawText("VAL " + std::to_string(build.value),
           glm::vec2{r.x + 6.f, r.y + 22.f}, r.w / 14.f,
           glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
}

void KasinoGame::drawScoreboard() {
  float width = m_Camera.LogicalWidth();
  Rect bar{0.f, 0.f, width, m_ScoreboardHeight};
  Render2D::DrawQuad(glm::vec2{bar.x, bar.y}, glm::vec2{bar.w, bar.h},
                     glm::vec4(0.07f, 0.18f, 0.11f, 1.0f));
  Render2D::DrawQuad(glm::vec2{bar.x, bar.y}, glm::vec2{bar.w, 4.f},
                     glm::vec4(0.02f, 0.05f, 0.03f, 1.0f));

  drawText("ROUND " + std::to_string(m_RoundNumber), glm::vec2{12.f, 14.f}, 4.f,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  drawText("DECK " + std::to_string(m_State.stock.size()),
           glm::vec2{width - 120.f, 14.f}, 4.f,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));

  for (int p = 0; p < m_State.numPlayers; ++p) {
    float offsetX = 12.f + p * (width * 0.5f);
    glm::vec4 color = m_PlayerColors[p % m_PlayerColors.size()];
    drawText("PLAYER " + std::to_string(p + 1), glm::vec2{offsetX, 44.f}, 3.5f,
             color);
    int total = (p < (int)m_TotalScores.size()) ? m_TotalScores[p] : 0;
    drawText("TOTAL " + std::to_string(total), glm::vec2{offsetX, 64.f}, 3.f,
             glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    drawText("SWEEPS " + std::to_string(m_State.players[p].sweeps),
             glm::vec2{offsetX + 120.f, 64.f}, 3.f,
             glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  }

  drawText("TURN P" + std::to_string(m_State.current + 1),
           glm::vec2{width * 0.5f - 60.f, 14.f}, 4.f,
           m_PlayerColors[m_State.current % m_PlayerColors.size()]);
}

void KasinoGame::drawHands() {
  for (int p = 0; p < m_State.numPlayers; ++p) {
    const auto &hand = m_State.players[p].hand;
    const auto &rects = m_PlayerHandRects[p];
    for (size_t i = 0; i < hand.size(); ++i) {
      bool isCurrent = (p == m_State.current);
      bool selected =
          (isCurrent && m_Selection.handIndex &&
           *m_Selection.handIndex == static_cast<int>(i));
      drawCardFace(hand[i], rects[i], isCurrent, selected, false, false);
    }
  }
}

void KasinoGame::drawTable() {
  Render2D::DrawQuad(glm::vec2{m_TableRect.x - 8.f, m_TableRect.y - 8.f},
                     glm::vec2{m_TableRect.w + 16.f, m_TableRect.h + 16.f},
                     glm::vec4(0.05f, 0.12f, 0.05f, 1.0f));
  Render2D::DrawQuad(glm::vec2{m_TableRect.x, m_TableRect.y},
                     glm::vec2{m_TableRect.w, m_TableRect.h},
                     glm::vec4(0.12f, 0.35f, 0.16f, 1.0f));

  const auto &loose = m_State.table.loose;
  for (size_t i = 0; i < loose.size(); ++i) {
    bool selected = m_Selection.loose.count(static_cast<int>(i)) != 0;
    bool legal = i < m_LooseHighlights.size() ? m_LooseHighlights[i] : false;
    bool hovered = m_HoveredLoose.count(static_cast<int>(i)) != 0;
    drawCardFace(loose[i], m_LooseRects[i], true, selected, legal, hovered);
  }

  const auto &builds = m_State.table.builds;
  for (size_t i = 0; i < builds.size(); ++i) {
    bool selected = m_Selection.builds.count(static_cast<int>(i)) != 0;
    bool legal = i < m_BuildHighlights.size() ? m_BuildHighlights[i] : false;
    bool hovered = m_HoveredBuilds.count(static_cast<int>(i)) != 0;
    drawBuildFace(builds[i], m_BuildRects[i], legal, hovered, selected);
  }
}

void KasinoGame::drawActionPanel() {
  Render2D::DrawQuad(glm::vec2{m_ActionPanelRect.x - 4.f,
                               m_ActionPanelRect.y - 4.f},
                     glm::vec2{m_ActionPanelRect.w + 8.f,
                               m_ActionPanelRect.h + 8.f},
                     glm::vec4(0.05f, 0.08f, 0.09f, 1.0f));
  Render2D::DrawQuad(glm::vec2{m_ActionPanelRect.x, m_ActionPanelRect.y},
                     glm::vec2{m_ActionPanelRect.w, m_ActionPanelRect.h},
                     glm::vec4(0.10f, 0.18f, 0.22f, 1.0f));

  drawText("ACTIONS", glm::vec2{m_ActionPanelRect.x + 10.f,
                                m_ActionPanelRect.y + 6.f},
           3.f, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));

  for (size_t i = 0; i < m_ActionEntries.size(); ++i) {
    const auto &entry = m_ActionEntries[i];
    glm::vec4 color =
        (m_HoveredAction == static_cast<int>(i))
            ? glm::vec4(0.9f, 0.6f, 0.3f, 1.0f)
            : glm::vec4(0.2f, 0.3f, 0.35f, 1.0f);
    Render2D::DrawQuad(glm::vec2{entry.rect.x, entry.rect.y},
                       glm::vec2{entry.rect.w, entry.rect.h}, color);
    drawText(entry.label,
             glm::vec2{entry.rect.x + 6.f, entry.rect.y + 10.f}, 3.f,
             glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  }

  if (m_Selection.handIndex) {
    Render2D::DrawQuad(glm::vec2{m_CancelButtonRect.x, m_CancelButtonRect.y},
                       glm::vec2{m_CancelButtonRect.w, m_CancelButtonRect.h},
                       glm::vec4(0.35f, 0.18f, 0.18f, 1.0f));
    drawText("CANCEL", glm::vec2{m_CancelButtonRect.x + 12.f,
                                 m_CancelButtonRect.y + 8.f},
             3.2f, glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  }
}

void KasinoGame::drawPromptOverlay() {
  if (!m_ShowPrompt) return;
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  Render2D::DrawQuad(glm::vec2{0.f, 0.f}, glm::vec2{width, height},
                     glm::vec4(0.f, 0.f, 0.f, 0.55f));

  Render2D::DrawQuad(glm::vec2{m_PromptBoxRect.x - 4.f, m_PromptBoxRect.y - 4.f},
                     glm::vec2{m_PromptBoxRect.w + 8.f, m_PromptBoxRect.h + 8.f},
                     glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  Render2D::DrawQuad(glm::vec2{m_PromptBoxRect.x, m_PromptBoxRect.y},
                     glm::vec2{m_PromptBoxRect.w, m_PromptBoxRect.h},
                     glm::vec4(0.12f, 0.16f, 0.18f, 1.0f));

  drawText(m_PromptHeader,
           glm::vec2{m_PromptBoxRect.x + 16.f, m_PromptBoxRect.y + 20.f}, 4.f,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));

  if (!m_LastRoundScores.empty()) {
    float lineY = m_PromptBoxRect.y + 60.f;
    for (int p = 0; p < m_State.numPlayers; ++p) {
      if (p >= (int)m_LastRoundScores.size()) break;
      const auto &line = m_LastRoundScores[p];
      int total = (p < (int)m_TotalScores.size()) ? m_TotalScores[p] : 0;
      std::string text = "P" + std::to_string(p + 1) + " ROUND " +
                         std::to_string(line.total) + " TOTAL " +
                         std::to_string(total);
      drawText(text, glm::vec2{m_PromptBoxRect.x + 16.f, lineY}, 3.2f,
               glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
      lineY += 24.f;
    }
  }

  Render2D::DrawQuad(glm::vec2{m_PromptButtonRect.x, m_PromptButtonRect.y},
                     glm::vec2{m_PromptButtonRect.w, m_PromptButtonRect.h},
                     glm::vec4(0.25f, 0.55f, 0.85f, 1.0f));
  drawText(m_PromptButtonLabel,
           glm::vec2{m_PromptButtonRect.x + 12.f,
                     m_PromptButtonRect.y + 10.f},
           3.2f, glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
}

void KasinoGame::drawText(const std::string &text, glm::vec2 pos, float scale,
                          const glm::vec4 &color) {
  float x = pos.x;
  float y = pos.y;
  for (char ch : text) {
    if (ch == '\n') {
      y += scale * 6.f;
      x = pos.x;
      continue;
    }
    const Glyph &g = glyphFor(ch);
    for (int row = 0; row < 5; ++row) {
      const char *rowStr = g.rows[row];
      for (int col = 0; col < g.width && rowStr[col] != '\0'; ++col) {
        if (rowStr[col] != ' ' && rowStr[col] != '\0') {
          glm::vec2 cellPos{x + (float)col * scale, y + (float)row * scale};
          Render2D::DrawQuad(cellPos, glm::vec2{scale, scale}, color);
        }
      }
    }
    x += (float)g.width * scale + scale * 0.5f;
  }
}

void KasinoGame::drawScene() {
  drawScoreboard();
  drawHands();
  drawTable();
  drawActionPanel();
  drawPromptOverlay();
}

void KasinoGame::OnRender() { drawScene(); }

