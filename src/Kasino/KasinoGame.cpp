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

KasinoGame::~KasinoGame() = default;

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

  m_MenuSeatIsAI = {false, true, true, true};
  m_MenuSelectedPlayers = 2;
  updateMenuHumanCounts();
  m_HumanSeatCount = m_MenuSelectedHumans;
  m_SeatIsAI.clear();

  m_State.numPlayers = m_MenuSelectedPlayers;
  m_State.players.resize(m_State.numPlayers);
  m_Phase = Phase::MainMenu;
  m_PromptMode = PromptMode::None;
  m_ShowPrompt = false;
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
  m_Phase = Phase::Playing;
  m_ShowPrompt = false;
  m_PromptMode = PromptMode::None;
  updateRoundScorePreview();
  updateLayout();
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
  m_PromptMode = PromptMode::None;
  updateRoundScorePreview();
  updateLayout();
  refreshHighlights();
}

void KasinoGame::updateRoundScorePreview() {
  if (m_State.numPlayers <= 0) {
    m_CurrentRoundScores.clear();
    return;
  }

  Casino::GameState previewState = m_State;
  m_CurrentRoundScores = Casino::ScoreRound(previewState);
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

void KasinoGame::updateMainMenuLayout() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float buttonWidth = std::min(width * 0.45f, 280.f);
  float buttonHeight = 60.f;
  float startX = width * 0.5f - buttonWidth * 0.5f;
  float startY = height * 0.5f - buttonHeight - 24.f;

  m_MainMenuStartButtonRect = {startX, startY, buttonWidth, buttonHeight};
  m_MainMenuSettingsButtonRect =
      {startX, startY + buttonHeight + 28.f, buttonWidth, buttonHeight};

  updatePromptLayout();
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

  updatePromptLayout();
}

void KasinoGame::updatePromptLayout() {
  if (!m_ShowPrompt) {
    m_MenuPlayerCountRects.clear();
    m_MenuSeatToggleRects.clear();
    return;
  }

  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float boxWidth = width * 0.75f;
  float boxHeight = 220.f;

  switch (m_PromptMode) {
  case PromptMode::RoundSummary:
    boxHeight = 260.f;
    break;
  case PromptMode::MatchSummary:
    boxHeight = 220.f;
    break;
  case PromptMode::PlayerSetup: {
    float seatBlock = static_cast<float>(m_MenuSelectedPlayers) * 44.f;
    boxHeight = std::max(320.f, 260.f + seatBlock);
  } break;
  case PromptMode::Settings:
    boxHeight = 240.f;
    break;
  case PromptMode::None:
    break;
  }

  m_PromptBoxRect = {width * 0.5f - boxWidth * 0.5f,
                     height * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight};
  m_PromptButtonRect = {m_PromptBoxRect.x + (boxWidth - 180.f) * 0.5f,
                        m_PromptBoxRect.y + boxHeight - 56.f, 180.f, 40.f};

  if (m_PromptMode == PromptMode::PlayerSetup) {
    m_MenuPlayerCountRects.clear();
    m_MenuPlayerCountRects.reserve(4);
    float optionWidth = 60.f;
    float optionHeight = 40.f;
    float optionSpacing = 16.f;
    float totalWidth = optionWidth * 4.f + optionSpacing * 3.f;
    float startX = m_PromptBoxRect.x + (boxWidth - totalWidth) * 0.5f;
    float optionY = m_PromptBoxRect.y + 90.f;
    for (int i = 0; i < 4; ++i) {
      m_MenuPlayerCountRects.push_back(
          {startX + (float)i * (optionWidth + optionSpacing), optionY,
           optionWidth, optionHeight});
    }

    m_MenuSeatToggleRects.clear();
    float seatWidth = boxWidth - 48.f;
    float seatHeight = 32.f;
    float seatY = optionY + optionHeight + 60.f;
    for (int i = 0; i < m_MenuSelectedPlayers; ++i) {
      m_MenuSeatToggleRects.push_back(
          {m_PromptBoxRect.x + 24.f, seatY, seatWidth, seatHeight});
      seatY += seatHeight + 12.f;
    }
  } else {
    m_MenuPlayerCountRects.clear();
    m_MenuSeatToggleRects.clear();
  }
}

void KasinoGame::updateMenuHumanCounts() {
  m_MenuSelectedHumans = 0;
  for (int i = 0; i < m_MenuSelectedPlayers && i < 4; ++i) {
    if (!m_MenuSeatIsAI[i]) ++m_MenuSelectedHumans;
  }
  if (m_MenuSelectedHumans == 0 && m_MenuSelectedPlayers > 0) {
    m_MenuSeatIsAI[0] = false;
    m_MenuSelectedHumans = 1;
  }
}

void KasinoGame::layoutActionEntries() {
  float buttonHeight = 40.f;
  float x = m_ActionPanelRect.x + 12.f;
  float labelScale = 3.f;
  float labelTop = m_ActionPanelRect.y + 6.f;
  float labelHeight = 5.f * labelScale;
  float y = labelTop + labelHeight + 8.f;
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

void KasinoGame::processMainMenuInput(float mx, float my) {
  if (!m_Input) return;
  bool click = m_Input->WasMousePressed(MouseButton::Left);
  if (!click) return;

  if (m_MainMenuStartButtonRect.Contains(mx, my)) {
    m_ShowPrompt = true;
    m_PromptMode = PromptMode::PlayerSetup;
    m_PromptHeader = "START NEW MATCH";
    m_PromptButtonLabel = "START MATCH";
    updatePromptLayout();
  } else if (m_MainMenuSettingsButtonRect.Contains(mx, my)) {
    m_ShowPrompt = true;
    m_PromptMode = PromptMode::Settings;
    m_PromptHeader = "SETTINGS";
    m_PromptButtonLabel = "CLOSE";
    updatePromptLayout();
  }
}

bool KasinoGame::handlePromptInput(float mx, float my) {
  if (!m_ShowPrompt || !m_Input) return false;

  bool click = m_Input->WasMousePressed(MouseButton::Left);
  bool handled = false;

  if (m_PromptMode == PromptMode::PlayerSetup) {
    if (click) {
      for (size_t i = 0; i < m_MenuPlayerCountRects.size(); ++i) {
        if (m_MenuPlayerCountRects[i].Contains(mx, my)) {
          int newCount = static_cast<int>(i) + 1;
          newCount = std::clamp(newCount, 1, 4);
          if (newCount != m_MenuSelectedPlayers) {
            m_MenuSelectedPlayers = newCount;
            for (int seat = m_MenuSelectedPlayers; seat < 4; ++seat)
              m_MenuSeatIsAI[seat] = true;
            if (m_MenuSelectedPlayers > 0)
              m_MenuSeatIsAI[0] = false;
            updateMenuHumanCounts();
            updatePromptLayout();
          }
          handled = true;
          break;
        }
      }

      if (!handled) {
        for (size_t i = 0; i < m_MenuSeatToggleRects.size(); ++i) {
          if (!m_MenuSeatToggleRects[i].Contains(mx, my)) continue;
          int seat = static_cast<int>(i);
          if (seat < m_MenuSelectedPlayers) {
            if (m_MenuSeatIsAI[seat]) {
              m_MenuSeatIsAI[seat] = false;
            } else {
              if (m_MenuSelectedHumans > 1) {
                m_MenuSeatIsAI[seat] = true;
              }
            }
            updateMenuHumanCounts();
            handled = true;
          }
          break;
        }
      }
    }
  }

  if (click && m_PromptButtonRect.Contains(mx, my)) {
    handlePrompt();
    handled = true;
  }

  return handled;
}

void KasinoGame::applyMove(const Casino::Move &mv) {
  if (!Casino::ApplyMove(m_State, mv)) return;

  m_Selection.Clear();
  m_ActionEntries.clear();
  updateLegalMoves();
  layoutActionEntries();
  updateLayout();
  refreshHighlights();

  if (m_State.RoundOver()) {
    handleRoundEnd();
    updateLayout();
    refreshHighlights();
  } else {
    updateRoundScorePreview();
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

  m_CurrentRoundScores.clear();

  m_Phase = hasWinner ? Phase::MatchSummary : Phase::RoundSummary;
  m_PromptMode = hasWinner ? PromptMode::MatchSummary : PromptMode::RoundSummary;
  m_ShowPrompt = true;
  if (hasWinner) {
    m_PromptHeader = "PLAYER " + std::to_string(m_WinningPlayer + 1) +
                     " WINS";
    m_PromptButtonLabel = "NEW MATCH";
  } else {
    m_PromptHeader = "ROUND " + std::to_string(m_RoundNumber) + " COMPLETE";
    m_PromptButtonLabel = "NEXT ROUND";
  }
  updatePromptLayout();
}

void KasinoGame::handlePrompt() {
  switch (m_PromptMode) {
  case PromptMode::MatchSummary:
    startNewMatch();
    break;
  case PromptMode::RoundSummary:
    startNextRound();
    break;
  case PromptMode::PlayerSetup: {
    updateMenuHumanCounts();
    if (m_MenuSelectedHumans <= 0 || m_MenuSelectedPlayers <= 0) return;
    m_State.numPlayers = m_MenuSelectedPlayers;
    m_State.players.resize(m_State.numPlayers);
    m_SeatIsAI.assign(m_State.numPlayers, false);
    for (int i = 0; i < m_State.numPlayers && i < 4; ++i) {
      m_SeatIsAI[i] = m_MenuSeatIsAI[i];
    }
    m_HumanSeatCount = m_MenuSelectedHumans;
    startNewMatch();
  } break;
  case PromptMode::Settings:
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    break;
  case PromptMode::None:
    m_ShowPrompt = false;
    break;
  }
}

void KasinoGame::OnUpdate(float dt) {
  (void)dt;
  if (!m_Input) return;

  if (m_Phase == Phase::MainMenu) {
    updateMainMenuLayout();
  } else {
    updateLayout();
    refreshHighlights();
  }

  float mx = m_Input->MouseX();
  float my = m_Input->MouseY();
  m_LastMousePos = {mx, my};

  if (m_Phase == Phase::MainMenu) {
    m_MainMenuStartHovered = m_MainMenuStartButtonRect.Contains(mx, my);
    m_MainMenuSettingsHovered =
        m_MainMenuSettingsButtonRect.Contains(mx, my);
  } else {
    updateHoveredAction(mx, my);
    m_MainMenuStartHovered = false;
    m_MainMenuSettingsHovered = false;
  }

  if (m_ShowPrompt) {
    handlePromptInput(mx, my);
  } else if (m_Phase == Phase::MainMenu) {
    processMainMenuInput(mx, my);
  } else {
    processInput(mx, my);
    if (m_Phase == Phase::Playing && m_State.RoundOver()) {
      handleRoundEnd();
    }
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
    if (m_Phase == Phase::Playing &&
        p < static_cast<int>(m_CurrentRoundScores.size())) {
      total += m_CurrentRoundScores[p].total;
    }
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

  if (m_PromptMode == PromptMode::RoundSummary ||
      m_PromptMode == PromptMode::MatchSummary) {
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
  } else if (m_PromptMode == PromptMode::PlayerSetup) {
    drawText("SELECT TOTAL PLAYERS", glm::vec2{m_PromptBoxRect.x + 16.f,
                                              m_PromptBoxRect.y + 64.f},
             3.f, glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));
    for (size_t i = 0; i < m_MenuPlayerCountRects.size(); ++i) {
      const Rect &rect = m_MenuPlayerCountRects[i];
      bool selected = (static_cast<int>(i) + 1) == m_MenuSelectedPlayers;
      bool hovered = rect.Contains(m_LastMousePos.x, m_LastMousePos.y);
      glm::vec4 color = selected ? glm::vec4(0.28f, 0.58f, 0.38f, 1.0f)
                                 : glm::vec4(0.2f, 0.3f, 0.35f, 1.0f);
      if (hovered) {
        color = glm::mix(color, glm::vec4(0.9f, 0.7f, 0.35f, 1.0f), 0.35f);
      }
      Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                         color);
      std::string label = std::to_string(static_cast<int>(i) + 1);
      float textX = rect.x + rect.w * 0.5f - (float)label.size() * 3.f;
      drawText(label, glm::vec2{textX, rect.y + 10.f}, 3.2f,
               glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    }

    drawText("ASSIGN SEAT ROLES", glm::vec2{m_PromptBoxRect.x + 16.f,
                                            m_PromptBoxRect.y + 160.f},
             3.f, glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));
    for (size_t i = 0; i < m_MenuSeatToggleRects.size(); ++i) {
      const Rect &rect = m_MenuSeatToggleRects[i];
      bool isAI = m_MenuSeatIsAI[i];
      bool hovered = rect.Contains(m_LastMousePos.x, m_LastMousePos.y);
      glm::vec4 color = isAI ? glm::vec4(0.25f, 0.28f, 0.36f, 1.0f)
                             : glm::vec4(0.25f, 0.55f, 0.38f, 1.0f);
      if (hovered) {
        color = glm::mix(color, glm::vec4(0.85f, 0.65f, 0.3f, 1.0f), 0.25f);
      }
      Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                         color);
      std::string label = "SEAT " + std::to_string(static_cast<int>(i) + 1) +
                          " - " + (isAI ? "AI" : "HUMAN");
      drawText(label, glm::vec2{rect.x + 12.f, rect.y + 8.f}, 3.f,
               glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    }

    int aiCount = std::max(0, m_MenuSelectedPlayers - m_MenuSelectedHumans);
    drawText("HUMANS " + std::to_string(m_MenuSelectedHumans) + " | AI " +
                 std::to_string(aiCount),
             glm::vec2{m_PromptBoxRect.x + 16.f,
                       m_PromptButtonRect.y - 48.f},
             3.f, glm::vec4(0.8f, 0.85f, 0.9f, 1.0f));
    drawText("CLICK TO TOGGLE HUMAN / AI",
             glm::vec2{m_PromptBoxRect.x + 16.f,
                       m_PromptButtonRect.y - 24.f},
             2.8f, glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));
  } else if (m_PromptMode == PromptMode::Settings) {
    drawText("OPTIONS COMING SOON",
             glm::vec2{m_PromptBoxRect.x + 16.f, m_PromptBoxRect.y + 64.f},
             3.2f, glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));
    drawText("AUDIO, RULES, AND MORE WILL LIVE HERE",
             glm::vec2{m_PromptBoxRect.x + 16.f, m_PromptBoxRect.y + 96.f},
             3.f, glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));
  }

  if (!m_PromptButtonLabel.empty()) {
    bool hovered = m_PromptButtonRect.Contains(m_LastMousePos.x, m_LastMousePos.y);
    bool enabled = true;
    if (m_PromptMode == PromptMode::PlayerSetup) {
      enabled = m_MenuSelectedHumans > 0 && m_MenuSelectedPlayers > 0;
    }
    glm::vec4 baseColor = enabled ? glm::vec4(0.25f, 0.55f, 0.85f, 1.0f)
                                  : glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
    if (hovered && enabled) {
      baseColor = glm::vec4(0.35f, 0.65f, 0.95f, 1.0f);
    }
    Render2D::DrawQuad(glm::vec2{m_PromptButtonRect.x, m_PromptButtonRect.y},
                       glm::vec2{m_PromptButtonRect.w, m_PromptButtonRect.h},
                       baseColor);
    drawText(m_PromptButtonLabel,
             glm::vec2{m_PromptButtonRect.x + 12.f,
                       m_PromptButtonRect.y + 10.f},
             3.2f, glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  }
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
  if (m_Phase == Phase::MainMenu) {
    drawMainMenu();
  } else {
    drawScoreboard();
    drawHands();
    drawTable();
    drawActionPanel();
  }
  drawPromptOverlay();
}

void KasinoGame::drawMainMenu() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();

  Render2D::DrawQuad(glm::vec2{0.f, 0.f}, glm::vec2{width, height},
                     glm::vec4(0.06f, 0.12f, 0.15f, 1.0f));

  drawText("KASINO", glm::vec2{width * 0.5f - 90.f, height * 0.25f - 40.f}, 6.5f,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  drawText("CLASSIC TABLE PLAY", glm::vec2{width * 0.5f - 140.f,
                                          height * 0.25f + 32.f},
           3.2f, glm::vec4(0.8f, 0.85f, 0.9f, 1.0f));

  auto drawButton = [&](const Rect &rect, const std::string &label,
                        bool hovered) {
    glm::vec4 baseColor = hovered ? glm::vec4(0.30f, 0.55f, 0.78f, 1.0f)
                                  : glm::vec4(0.18f, 0.32f, 0.38f, 1.0f);
    Render2D::DrawQuad(glm::vec2{rect.x - 4.f, rect.y - 4.f},
                       glm::vec2{rect.w + 8.f, rect.h + 8.f},
                       glm::vec4(0.03f, 0.05f, 0.06f, 1.0f));
    Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                       baseColor);
    float textX = rect.x + rect.w * 0.5f - (float)label.size() * 3.f;
    float textY = rect.y + rect.h * 0.5f - 12.f;
    drawText(label, glm::vec2{textX, textY}, 4.f,
             glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  };

  drawButton(m_MainMenuStartButtonRect, "START", m_MainMenuStartHovered);
  drawButton(m_MainMenuSettingsButtonRect, "SETTINGS",
             m_MainMenuSettingsHovered);

  drawText("CHOOSE AN OPTION TO BEGIN",
           glm::vec2{width * 0.5f - 160.f,
                     m_MainMenuSettingsButtonRect.y +
                         m_MainMenuSettingsButtonRect.h + 36.f},
           3.f, glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));
}

void KasinoGame::OnRender() { drawScene(); }

