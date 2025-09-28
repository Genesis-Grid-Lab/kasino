#include "Kasino/KasinoGame.h"

#include "app/Game.h"
#include "core/Factory.h"
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
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

KasinoGame::~KasinoGame() = default;

namespace {

struct Glyph {
  int width = 3;
  std::array<const char *, 5> rows{};
};

constexpr float kAiDecisionDelay = 0.5f;
constexpr float kAiAnimDuration = 0.3f;
constexpr float kDealAnimDuration = 0.35f;
constexpr float kDealDelayStep = 0.12f;

const std::string kMainMenuTitleText = "KASINO";
const std::string kMainMenuSubtitleText = "CLASSIC TABLE PLAY";
const std::string kMainMenuFooterText = "CHOOSE AN OPTION TO BEGIN";
constexpr float kMainMenuTitleScale = 6.5f;
constexpr float kMainMenuSubtitleScale = 3.2f;
constexpr float kMainMenuFooterScale = 3.f;
constexpr float kTitleSubtitleSpacingFactor = 1.2f;
constexpr float kSubtitleButtonsSpacingFactor = 3.0f;
constexpr float kButtonsFooterSpacingFactor = 2.4f;
constexpr float kButtonVerticalSpacingFactor = 1.75f;

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

struct PreviewScoreResult {
  std::vector<Casino::ScoreLine> lines;
  bool includeMajorities = false;
};

PreviewScoreResult computePreviewScores(const Casino::GameState &state) {
  PreviewScoreResult result;
  result.includeMajorities = state.RoundOver();
  Casino::GameState previewState = state;
  result.lines = Casino::ScoreRound(previewState);
  if (!result.includeMajorities) {
    for (auto &line : result.lines) {
      line.total -= line.mostCards;
      line.total -= line.mostSpades;
    }
  }
  return result;
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


}  // namespace
glm::mat4 KasinoGame::buildCardTransform(const KasinoGame::Rect &rect, float rotation) {
  glm::vec2 size(rect.w, rect.h);
  glm::vec2 pos(rect.x, rect.y);
  glm::vec2 center = size * 0.5f;
  glm::vec3 pos3(pos, 0.0f);
  glm::vec3 center3(center, 0.0f);
  glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos3);
  transform = glm::translate(transform, center3);
  transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
  transform = glm::translate(transform, -center3);
  transform = glm::scale(transform, glm::vec3(size, 1.0f));
  return transform;
}

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

  loadCardTextures();

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
  m_IsAiPlayer.clear();

  m_State.numPlayers = m_MenuSelectedPlayers;
  m_State.players.resize(m_State.numPlayers);
  m_Phase = Phase::MainMenu;
  m_PromptMode = PromptMode::None;
  m_ShowPrompt = false;
  return true;
}

void KasinoGame::loadCardTextures() {
  m_CardTextures.clear();

  const std::array<Casino::Suit, 4> suits = {Casino::Suit::Clubs,
                                             Casino::Suit::Diamonds,
                                             Casino::Suit::Hearts,
                                             Casino::Suit::Spades};

  for (Casino::Suit suit : suits) {
    for (int value = Casino::RankValue(Casino::Rank::Ace);
         value <= Casino::RankValue(Casino::Rank::King); ++value) {
      Casino::Rank rank = static_cast<Casino::Rank>(value);
      Casino::Card card(rank, suit);
      auto texture = Factory::CreateTexture2D();
      if (!texture) continue;

      std::string path = cardTexturePath(card);
      if (texture->LoadFromFile(path.c_str(), false)) {
        m_CardTextures[cardTextureKey(card)] = texture;
      }
    }
  }

  const std::string backPath =
      "Resources/Cards/Standard/rect_cards/individual/card back/card_back_rect_1.png";
  auto backTexture = Factory::CreateTexture2D();
  if (backTexture && backTexture->LoadFromFile(backPath.c_str(), true)) {
    m_CardBackTexture = backTexture;
  } else {
    m_CardBackTexture.reset();
  }
}

std::string KasinoGame::cardTextureKey(const Casino::Card &card) const {
  return card.ToString();
}

std::string KasinoGame::cardTexturePath(const Casino::Card &card) const {
  std::string folder = cardSuitFolder(card.suit);
  return "Resources/Cards/Standard/rect_cards/individual/" + folder + "/" +
         cardRankString(card.rank) + folder + ".png";
}

std::string KasinoGame::cardRankString(Casino::Rank rank) const {
  switch (rank) {
  case Casino::Rank::Ace:
    return "A";
  case Casino::Rank::Jack:
    return "J";
  case Casino::Rank::Queen:
    return "Q";
  case Casino::Rank::King:
    return "K";
  default:
    return std::to_string(Casino::RankValue(rank));
  }
}

std::string KasinoGame::cardSuitFolder(Casino::Suit suit) const {
  switch (suit) {
  case Casino::Suit::Clubs:
    return "club";
  case Casino::Suit::Diamonds:
    return "diamond";
  case Casino::Suit::Hearts:
    return "heart";
  case Casino::Suit::Spades:
    return "spade";
  }
  return "";
}

void KasinoGame::OnStop() {
  m_Input.reset();
  m_CardTextures.clear();
  m_CardBackTexture.reset();
  Render2D::Shutdown();
}

void KasinoGame::startNewMatch() {
  m_IsAiPlayer.assign(m_State.numPlayers, true);
  for (int i = 0; i < m_State.numPlayers; ++i) {
    bool isAi = true;
    if (i < static_cast<int>(m_SeatIsAI.size())) {
      isAi = m_SeatIsAI[i];
    } else if (i == 0) {
      isAi = false;
    }
    m_IsAiPlayer[i] = isAi;
  }
  if (!m_IsAiPlayer.empty()) {
    m_IsAiPlayer[0] = false;
  }

  m_TotalScores.assign(m_State.numPlayers, 0);
  m_RoundNumber = 1;
  m_WinningPlayer = -1;
  Casino::StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = Casino::LegalMoves(m_State);
  m_Selection.Clear();
  m_ActionEntries.clear();
  m_LastRoundScores.clear();
  m_PendingAiMove.reset();
  m_PendingLooseHighlights.clear();
  m_PendingBuildHighlights.clear();
  m_AiDecisionTimer = 0.f;
  m_AiAnimProgress = 0.f;
  m_PendingAiPlayer = -1;
  m_PendingAiHandIndex = -1;
  m_Phase = Phase::Playing;
  m_ShowPrompt = false;
  m_PromptMode = PromptMode::None;
  updateRoundScorePreview();
  updateLayout();
  beginDealAnimation();
  if (!m_IsDealing) {
    refreshHighlights();
  }
}

void KasinoGame::startNextRound() {
  ++m_RoundNumber;
  Casino::StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = Casino::LegalMoves(m_State);
  m_Selection.Clear();
  m_ActionEntries.clear();
  m_LastRoundScores.clear();
  m_PendingAiMove.reset();
  m_PendingLooseHighlights.clear();
  m_PendingBuildHighlights.clear();
  m_AiDecisionTimer = 0.f;
  m_AiAnimProgress = 0.f;
  m_PendingAiPlayer = -1;
  m_PendingAiHandIndex = -1;
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

  PreviewScoreResult preview = computePreviewScores(m_State);
  m_CurrentRoundScores.clear();
  m_CurrentRoundScores.reserve(preview.lines.size());

  for (const auto &line : preview.lines) {
    RunningScore running;
    running.line = line;
    running.potentialMajorities = line.mostCards + line.mostSpades;
    running.securedPoints =
        line.aces + line.bigCasino + line.littleCasino + line.sweeps;
    running.showMajorityBonuses = preview.includeMajorities;
    if (!preview.includeMajorities) {
      running.line.total = running.securedPoints;
    }
    m_CurrentRoundScores.push_back(running);
  }
}

void KasinoGame::beginDealAnimation() {
  m_DealQueue.clear();
  m_DealtCounts.assign(m_State.numPlayers, 0);
  m_IsDealing = false;

  float width = m_Camera.LogicalWidth();
  float deckX = width - m_CardWidth * 0.5f - 32.f;
  float deckY = m_ScoreboardHeight * 0.5f;
  m_DeckOrigin = glm::vec2(deckX, deckY);

  size_t maxHandSize = 0;
  for (const auto &player : m_State.players) {
    maxHandSize = std::max(maxHandSize, player.hand.size());
  }

  float currentDelay = 0.f;
  for (size_t cardIndex = 0; cardIndex < maxHandSize; ++cardIndex) {
    for (int p = 0; p < m_State.numPlayers; ++p) {
      const auto &hand = m_State.players[p].hand;
      if (cardIndex >= hand.size()) {
        continue;
      }

      DealAnim anim;
      anim.player = p;
      anim.handIndex = static_cast<int>(cardIndex);
      anim.card = hand[cardIndex];
      anim.delay = currentDelay;
      anim.progress = 0.f;
      m_DealQueue.push_back(anim);
      currentDelay += kDealDelayStep;
    }
  }

  if (!m_DealQueue.empty()) {
    m_IsDealing = true;
    m_Selection.Clear();
    m_ActionEntries.clear();
    layoutActionEntries();
    m_HoveredAction = -1;
  } else {
    for (int p = 0; p < m_State.numPlayers; ++p) {
      if (p < static_cast<int>(m_State.players.size())) {
        m_DealtCounts[p] = static_cast<int>(m_State.players[p].hand.size());
      }
    }
  }
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
  glm::vec2 titleMetrics = measureText(kMainMenuTitleText, kMainMenuTitleScale);
  glm::vec2 subtitleMetrics =
      measureText(kMainMenuSubtitleText, kMainMenuSubtitleScale);
  float titleTop = height * 0.25f - titleMetrics.y;
  float titleToSubtitleSpacing = titleMetrics.y * kTitleSubtitleSpacingFactor;
  float subtitleTop = titleTop + titleMetrics.y + titleToSubtitleSpacing;
  float subtitleToButtonsSpacing =
      subtitleMetrics.y * kSubtitleButtonsSpacingFactor;
  float buttonsTop = subtitleTop + subtitleMetrics.y + subtitleToButtonsSpacing;
  float buttonSpacing = subtitleMetrics.y * kButtonVerticalSpacingFactor;
  float buttonWidth = std::min(width * 0.45f, 280.f);
  float buttonHeight = 60.f;
  float startX = width * 0.5f - buttonWidth * 0.5f;
  float startY = buttonsTop;

  m_MainMenuStartButtonRect = {startX, startY, buttonWidth, buttonHeight};
  m_MainMenuSettingsButtonRect =
      {startX, startY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight};

  updatePromptLayout();
}

void KasinoGame::updateLayout() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float margin = 16.f;
  float panelWidth = 160.f;
  float sideSeatVisibleFraction = 0.2f;
  float topSeatVisibleFraction = 0.2f;
  float sideSeatPeek = m_CardWidth * sideSeatVisibleFraction + margin;  

  bool hasLeftSeat = m_State.numPlayers >= 3;
  bool hasRightSeat = m_State.numPlayers >= 4;
  bool hasTopSeat = m_State.numPlayers >= 2;
  float sideSeatWidth = m_CardHeight + margin * 2.f;

  float actionPanelX = width - panelWidth - margin;
  float tableLeft = margin + (hasLeftSeat ? sideSeatPeek : 0.f);
  float tableRightLimit = actionPanelX - margin;
  if (hasRightSeat) {
    tableRightLimit -= sideSeatPeek;
    actionPanelX -= sideSeatPeek;
  }
  if (tableRightLimit <= tableLeft) {
    tableRightLimit = tableLeft + 160.f;
  }

  m_TableRect.x = tableLeft;
  m_TableRect.w = tableRightLimit - tableLeft;
  if (m_TableRect.w < 0.f) {
    m_TableRect.w = 0.f;
  }

  float tableTop = m_ScoreboardHeight + margin + (hasTopSeat ? (m_CardHeight + margin) : 0.f);
  float bottomSeatPadding = m_CardHeight + margin * 2.f;
  float tableBottomLimit = height - bottomSeatPadding;
  if (tableBottomLimit <= tableTop + 160.f) {
    tableBottomLimit = tableTop + 160.f;
  }
  m_TableRect.y = tableTop;
  m_TableRect.h = tableBottomLimit - tableTop;
  if (m_TableRect.h < 0.f) {
    m_TableRect.h = 0.f;
  }

  m_ActionPanelRect.x = actionPanelX;
  m_ActionPanelRect.y = m_TableRect.y;
  m_ActionPanelRect.w = panelWidth;
  m_ActionPanelRect.h = m_TableRect.h;

  // Seat anchors and hand rects
  m_PlayerSeatLayouts.assign(m_State.numPlayers, {});
  m_PlayerHandRects.assign(m_State.numPlayers, {});

  auto buildHorizontalSeat = [&](int playerIndex, float y,
                                 float visibleFraction) {
    SeatLayout layout;
    layout.orientation = SeatOrientation::Horizontal;
    layout.anchor = {m_TableRect.x, y, m_TableRect.w, m_CardHeight};
    layout.visibleFraction = visibleFraction;
    m_PlayerSeatLayouts[playerIndex] = layout;
  };

  if (m_State.numPlayers > 0) {
    float desiredBottomY = height - margin - m_CardHeight;
    float bottomY = std::max(desiredBottomY, m_TableRect.y + m_TableRect.h + margin);
    if (bottomY + m_CardHeight > height - margin) {
      bottomY = height - margin - m_CardHeight;
    }
    buildHorizontalSeat(0, bottomY, 1.f);
  }

  if (hasTopSeat) {
    float topY =
        m_ScoreboardHeight - m_CardHeight * (1.f - topSeatVisibleFraction);
    buildHorizontalSeat(1, topY, topSeatVisibleFraction);
  }

  if (hasLeftSeat) {
    SeatLayout layout;
    layout.orientation = SeatOrientation::Vertical;
    layout.anchor = {margin, m_TableRect.y, sideSeatWidth, m_TableRect.h};
    layout.visibleFraction = sideSeatVisibleFraction;
    m_PlayerSeatLayouts[2] = layout;
  }

  if (hasRightSeat) {
    SeatLayout layout;
    layout.orientation = SeatOrientation::Vertical;
    layout.anchor = {m_ActionPanelRect.x - margin - sideSeatWidth, m_TableRect.y,
                     sideSeatWidth, m_TableRect.h};
    layout.visibleFraction = sideSeatVisibleFraction;
    m_PlayerSeatLayouts[3] = layout;
  }

  for (int p = 0; p < m_State.numPlayers; ++p) {
    const auto &hand = m_State.players[p].hand;
    auto &rects = m_PlayerHandRects[p];
    rects.clear();
    rects.reserve(hand.size());

    const auto &layout = m_PlayerSeatLayouts[p];
    if (layout.anchor.w <= 0.f || layout.anchor.h <= 0.f) {
      continue;
    }

    if (layout.orientation == SeatOrientation::Horizontal) {
      float spacing = m_CardWidth * 0.2f;
      float totalWidth = hand.empty()
                             ? 0.f
                             : (float)hand.size() * m_CardWidth +
                                   (float)(hand.size() - 1) * spacing;
      float startX = layout.anchor.x;
      if (layout.anchor.w > totalWidth) {
        startX += (layout.anchor.w - totalWidth) * 0.5f;
      }
      for (size_t i = 0; i < hand.size(); ++i) {
        rects.push_back(Rect{startX + (float)i * (m_CardWidth + spacing),
                             layout.anchor.y, m_CardWidth, m_CardHeight});
      }
    } else {
      float cardW = m_CardHeight;
      float cardH = m_CardWidth;
      float spacing = cardH * 0.2f;
      float totalHeight = hand.empty()
                              ? 0.f
                              : (float)hand.size() * cardH +
                                    (float)(hand.size() - 1) * spacing;
      float startY = layout.anchor.y;
      if (layout.anchor.h > totalHeight) {
        startY += (layout.anchor.h - totalHeight) * 0.5f;
      }
      float visibleFraction = layout.visibleFraction;
      visibleFraction = std::clamp(visibleFraction, 0.f, 1.f);
      bool isLeftSeat = layout.anchor.x < m_TableRect.x;
      float desiredDrawX =
          isLeftSeat ? margin - m_CardWidth * (1.f - visibleFraction)
                     : width - margin - m_CardWidth * visibleFraction;
      float x = desiredDrawX + m_CardWidth * 0.5f - cardW * 0.5f;
      for (size_t i = 0; i < hand.size(); ++i) {
        rects.push_back(Rect{x, startY + (float)i * (cardH + spacing), cardW,
                             cardH});
      }
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

  if (m_Phase != Phase::Playing)
    return;

  if (m_IsDealing)
    return;  

  if (m_State.current >= 0 &&
      m_State.current < static_cast<int>(m_IsAiPlayer.size()) &&
      m_IsAiPlayer[m_State.current]) {
    return;
  }

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
    if (p >= static_cast<int>(m_IsAiPlayer.size())) continue;
    if (m_IsAiPlayer[p]) continue;
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

bool KasinoGame::playAiTurn() {
  if (m_Phase != Phase::Playing) return false;
  if (m_State.RoundOver()) return false;
  if (m_State.current < 0 || m_State.current >= m_State.numPlayers) return false;
  if (m_State.current >= static_cast<int>(m_IsAiPlayer.size())) return false;
  if (!m_IsAiPlayer[m_State.current]) return false;
  if (m_PendingAiMove) return false;

  if (m_LegalMoves.empty()) {
    updateLegalMoves();
  }
  if (m_LegalMoves.empty()) return false;

  const Casino::Move *selected = nullptr;
  const Casino::Move *trailMove = nullptr;
  for (const auto &mv : m_LegalMoves) {
    if (mv.type == Casino::MoveType::Capture) {
      selected = &mv;
      break;
    }
    if (!trailMove && mv.type == Casino::MoveType::Trail) {
      trailMove = &mv;
    }
  }

  if (!selected) {
    if (trailMove) {
      selected = trailMove;
    } else {
      selected = &m_LegalMoves.front();
    }
  }

  Casino::Move chosen = *selected;
  beginAiMove(chosen);
  return true;
}

void KasinoGame::beginAiMove(const Casino::Move &mv) {
  m_PendingAiMove = mv;
  m_AiDecisionTimer = kAiDecisionDelay;
  m_AiAnimProgress = 0.f;
  m_PendingAiPlayer = m_State.current;
  m_PendingAiHandIndex = -1;

  m_PendingLooseHighlights.assign(m_LooseRects.size(), false);
  m_PendingBuildHighlights.assign(m_BuildRects.size(), false);

  if (m_PendingAiPlayer >= 0 &&
      m_PendingAiPlayer < static_cast<int>(m_State.players.size())) {
    const auto &hand = m_State.players[m_PendingAiPlayer].hand;
    for (size_t i = 0; i < hand.size(); ++i) {
      if (hand[i] == mv.handCard) {
        m_PendingAiHandIndex = static_cast<int>(i);
        break;
      }
    }
  }

  auto markHighlights = [](const std::vector<int> &indices,
                           std::vector<bool> &flags) {
    for (int idx : indices) {
      if (idx >= 0 && idx < static_cast<int>(flags.size())) {
        flags[idx] = true;
      }
    }
  };

  if (mv.type == Casino::MoveType::Capture) {
    markHighlights(mv.captureLooseIdx, m_PendingLooseHighlights);
    markHighlights(mv.captureBuildIdx, m_PendingBuildHighlights);
  } else if (mv.type == Casino::MoveType::Build) {
    markHighlights(mv.buildUseLooseIdx, m_PendingLooseHighlights);
  } else if (mv.type == Casino::MoveType::ExtendBuild) {
    markHighlights(mv.buildUseLooseIdx, m_PendingLooseHighlights);
    markHighlights(mv.captureBuildIdx, m_PendingBuildHighlights);
  }
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
  } else if (m_State.HandsEmpty()) {
    updateRoundScorePreview();
    m_ShowPrompt = true;
    m_PromptMode = PromptMode::HandSummary;
    m_PromptHeader = "HAND COMPLETE";
    m_PromptButtonLabel = "DEAL NEXT HAND";
    updatePromptLayout();
  } else {
    updateRoundScorePreview();
  }
}

void KasinoGame::handleRoundEnd() {
  m_LastRoundScores = Casino::ScoreRound(m_State);
  if (m_TotalScores.size() != static_cast<size_t>(m_State.numPlayers))
    m_TotalScores.assign(m_State.numPlayers, 0);

  // bool hasWinner = false;
  // m_WinningPlayer = -1;
  std::vector<int> leaders;
  int bestTotal = 0;
  bool haveLeader = false;
  for (int p = 0; p < m_State.numPlayers; ++p) {
    m_TotalScores[p] += m_LastRoundScores[p].total;
    // if (m_TotalScores[p] >= m_TargetScore) {
    //   if (!hasWinner || m_TotalScores[p] > m_TotalScores[m_WinningPlayer]) {
    //     hasWinner = true;
    //     m_WinningPlayer = p;
    //   }

    int playerTotal = m_TotalScores[p];
    if (!haveLeader || playerTotal > bestTotal) {
      bestTotal = playerTotal;
      leaders.clear();
      leaders.push_back(p);
      haveLeader = true;
    } else if (playerTotal == bestTotal) {
      leaders.push_back(p);
    }
  }

  bool deckEmpty = m_State.stock.empty();
  bool tie = leaders.size() > 1;
  m_WinningPlayer = tie || leaders.empty() ? -1 : leaders.front();

  m_CurrentRoundScores.clear();

  // m_Phase = hasWinner ? Phase::MatchSummary : Phase::RoundSummary;
  // m_PromptMode = hasWinner ? PromptMode::MatchSummary :
  // PromptMode::RoundSummary;
  m_Phase = deckEmpty ? Phase::MatchSummary : Phase::RoundSummary;
  m_PromptMode =
      deckEmpty ? PromptMode::MatchSummary : PromptMode::RoundSummary;
  
  m_ShowPrompt = true;
  // if (hasWinner) {
  //   m_PromptHeader = "PLAYER " + std::to_string(m_WinningPlayer + 1) +
  //                    " WINS";
  //   m_PromptButtonLabel = "NEW MATCH";
  if (deckEmpty) {
    if (tie) {
      m_PromptHeader = "MATCH ENDS IN A TIE";
    } else if (m_WinningPlayer >= 0) {
      m_PromptHeader = "PLAYER " + std::to_string(m_WinningPlayer + 1) +
                       " WINS THE MATCH";
    } else {
      m_PromptHeader = "MATCH COMPLETE";
    }
    m_PromptButtonLabel = "START NEW MATCH";
  } else {
    m_PromptHeader = "ROUND " + std::to_string(m_RoundNumber) + " COMPLETE";
    // m_PromptButtonLabel = "NEXT ROUND";
    m_PromptButtonLabel = "DEAL NEXT HAND";
  }
  updatePromptLayout();
}

void KasinoGame::handlePrompt() {
  switch (m_PromptMode) {
  case PromptMode::MatchSummary:
    // startNewMatch();
    break;
  case PromptMode::RoundSummary:
    startNextRound();
    break;
  case PromptMode::HandSummary: {
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    m_Phase = Phase::Playing; // tmpish
    if (!Casino::DealNextHands(m_State)) {
      handleRoundEnd();
      updateLayout();
      refreshHighlights();
      break;
    }
    // updateLegalMoves();
    m_LegalMoves = Casino::LegalMoves(m_State);
    m_Selection.Clear();
    m_ActionEntries.clear();
    layoutActionEntries();
    updateLayout();
    // refreshHighlights();
    beginDealAnimation();
    updateRoundScorePreview();
    if (!m_IsDealing) {
      refreshHighlights();
    }    
  } break;
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
  if (!m_Input)
    return;

  bool escapePressed = m_Input->WasKeyPressed(Key::Escape);
  if (escapePressed) {
    if (m_ShowPrompt && m_PromptMode == PromptMode::Settings) {
      m_ShowPrompt = false;
      m_PromptMode = PromptMode::None;
      updatePromptLayout();
    } else if (!m_ShowPrompt) {
      m_ShowPrompt = true;
      m_PromptMode = PromptMode::Settings;
      m_PromptHeader = "SETTINGS";
      m_PromptButtonLabel = "CLOSE";
      updatePromptLayout();
    }
  }

  bool dealFinishedThisFrame = false;
  if (!m_DealQueue.empty()) {
    for (auto it = m_DealQueue.begin(); it != m_DealQueue.end();) {
      DealAnim &anim = *it;
      if (anim.delay > 0.f) {
        anim.delay = std::max(0.f, anim.delay - dt);
        ++it;
        continue;
      }

      if (kDealAnimDuration > 0.f) {
        anim.progress =
	  std::min(1.f, anim.progress + dt / kDealAnimDuration);
      } else {
        anim.progress = 1.f;
      }

      if (anim.progress >= 1.f) {
        if (anim.player >= 0 &&
            anim.player < static_cast<int>(m_DealtCounts.size()) &&
            anim.player < static_cast<int>(m_State.players.size())) {
          int &count = m_DealtCounts[anim.player];
          int handSize =
	    static_cast<int>(m_State.players[anim.player].hand.size());
          if (count < handSize) {
            ++count;
          }
        }
        it = m_DealQueue.erase(it);
      } else {
        ++it;
      }
    }

    if (m_DealQueue.empty()) {
      dealFinishedThisFrame = true;
    } else {
      m_IsDealing = true;
    }
  } else if (m_IsDealing) {
    dealFinishedThisFrame = true;
  }

  if (dealFinishedThisFrame) {
    m_IsDealing = false;
    for (int p = 0; p < m_State.numPlayers; ++p) {
      if (p < static_cast<int>(m_State.players.size())) {
        m_DealtCounts[p] = static_cast<int>(m_State.players[p].hand.size());
      }
    }
    if (m_Phase == Phase::Playing) {
      updateLegalMoves();
      layoutActionEntries();
    }
  }

  if (m_PendingAiMove) {
    if (m_AiDecisionTimer > 0.f) {
      m_AiDecisionTimer = std::max(0.f, m_AiDecisionTimer - dt);
    } else {
      if (kAiAnimDuration > 0.f) {
        m_AiAnimProgress =
            std::min(1.f, m_AiAnimProgress + dt / kAiAnimDuration);
      } else {
        m_AiAnimProgress = 1.f;
      }

      if (m_AiAnimProgress >= 1.f && m_PendingAiMove) {
        Casino::Move move = *m_PendingAiMove;
        m_PendingAiMove.reset();
        m_AiAnimProgress = 0.f;
        m_AiDecisionTimer = 0.f;
        m_PendingAiPlayer = -1;
        m_PendingAiHandIndex = -1;
        m_PendingLooseHighlights.clear();
        m_PendingBuildHighlights.clear();
        applyMove(move);
      }
    }
  }

  if (m_Phase == Phase::MainMenu) {
    updateMainMenuLayout();
  } else {
    updateLayout();
    if (!m_IsDealing) {
      refreshHighlights();
    }
  }

  if (!m_ShowPrompt && m_Phase == Phase::Playing && !m_IsDealing) {
    bool aiTurn = (m_State.current >= 0 &&
                   m_State.current < m_State.numPlayers &&
                   m_State.current < static_cast<int>(m_IsAiPlayer.size()) &&
                   m_IsAiPlayer[m_State.current] && !m_State.RoundOver());
    if (aiTurn && !m_PendingAiMove) {
      playAiTurn();
    }
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
  drawCardFace(card, r, 0.f, isCurrent, selected, legal, hovered);
}

void KasinoGame::drawCardFace(const Casino::Card &card, const Rect &r,
                              float rotation, bool isCurrent, bool selected,
                              bool legal, bool hovered) {
  Rect borderRect{r.x - 2.f, r.y - 2.f, r.w + 4.f, r.h + 4.f};
  glm::mat4 borderTransform = buildCardTransform(borderRect, rotation);
  glm::mat4 cardTransform = buildCardTransform(r, rotation);

  glm::vec4 borderColor = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);

  glm::vec4 baseTint(1.0f);
  if (!isCurrent) {
    baseTint =
        glm::mix(glm::vec4(1.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.f), 0.35f);
  }

  bool drewTexture = false;
  auto textureIt = m_CardTextures.find(cardTextureKey(card));
  if (textureIt != m_CardTextures.end() && textureIt->second) {
    Render2D::DrawQuad(cardTransform, textureIt->second, 1.0f, baseTint);
    drewTexture = true;
  } else {

      Render2D::DrawQuad(borderTransform, borderColor);
    Render2D::DrawQuad(cardTransform, baseTint);
  }

  if (legal)
    Render2D::DrawQuad(cardTransform,
                       glm::vec4(0.2f, 0.45f, 0.9f, 0.25f));
  if (hovered)
    Render2D::DrawQuad(cardTransform,
                       glm::vec4(0.95f, 0.55f, 0.25f, 0.4f));
  if (selected)
    Render2D::DrawQuad(cardTransform,
                       glm::vec4(0.95f, 0.85f, 0.2f, 0.45f));

  if (!drewTexture && rotation == 0.f) {
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
}

void KasinoGame::drawCardBack(const Rect &r, bool isCurrent) {
  drawCardBack(r, isCurrent, 0.f);
}

void KasinoGame::drawCardBack(const Rect &r, bool isCurrent, float rotation) {
  Rect borderRect{r.x - 2.f, r.y - 2.f, r.w + 4.f, r.h + 4.f};
  glm::mat4 borderTransform = buildCardTransform(borderRect, rotation);
  glm::mat4 cardTransform = buildCardTransform(r, rotation);

  glm::vec4 borderColor = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);

  if (m_CardBackTexture) {
    Render2D::DrawQuad(cardTransform, m_CardBackTexture, 1.0f,
                       glm::vec4(1.0f));
    if (isCurrent) {
      Render2D::DrawQuad(cardTransform,
                         glm::vec4(0.95f, 0.75f, 0.35f, 0.35f));
    }
  } else {
      Render2D::DrawQuad(borderTransform, borderColor);
    glm::vec4 baseColor = glm::vec4(0.15f, 0.25f, 0.45f, 1.0f);
    if (isCurrent) {
      baseColor =
          glm::mix(baseColor, glm::vec4(0.9f, 0.6f, 0.2f, 1.0f), 0.35f);
    }

    Render2D::DrawQuad(cardTransform, baseColor);

    if (r.w > 0.f && r.h > 0.f) {
      auto drawInset = [&](float offsetX, float offsetY, float width,
                           float height, const glm::vec4 &color) {
        if (width <= 0.f || height <= 0.f) {
          return;
        }
        glm::mat4 local = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(offsetX / r.w, offsetY / r.h,
                                                   0.0f));
        local = glm::scale(local, glm::vec3(width / r.w, height / r.h, 1.0f));
        Render2D::DrawQuad(cardTransform * local, color);
      };

      glm::vec4 innerColor = glm::vec4(0.25f, 0.35f, 0.55f, 1.0f);
      drawInset(6.f, 6.f, r.w - 12.f, r.h - 12.f, innerColor);

      glm::vec4 accentColor = glm::vec4(0.85f, 0.85f, 0.9f, 0.35f);
      drawInset(r.w * 0.25f, 10.f, r.w * 0.5f, r.h - 20.f, accentColor);
    }
  }
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
    bool showPotential = false;
    int potentialPoints = 0;
    if (m_Phase == Phase::Playing &&
        p < static_cast<int>(m_CurrentRoundScores.size())) {
      const auto &running = m_CurrentRoundScores[p];
      int roundPoints = running.securedPoints;
      if (running.showMajorityBonuses) {
        roundPoints += running.potentialMajorities;
      } else if (running.potentialMajorities > 0) {
        showPotential = true;
        potentialPoints = running.potentialMajorities;
      }
      total += roundPoints;
    }
    glm::vec2 totalPos{offsetX, 64.f};
    glm::vec4 totalColor(0.95f, 0.95f, 0.95f, 1.0f);
    std::string totalText = "TOTAL " + std::to_string(total);
    drawText(totalText, totalPos, 3.f, totalColor);
    if (showPotential && potentialPoints > 0) {
      // float baseWidth = measureTextWidth(totalText, 3.f);
      float baseWidth = measureText(totalText, 3.f).x;      
      std::string potentialText = " +" + std::to_string(potentialPoints);
      glm::vec4 potentialColor(totalColor.r, totalColor.g, totalColor.b, 0.6f);
      drawText(potentialText,
               glm::vec2{totalPos.x + baseWidth, totalPos.y}, 3.f,
               potentialColor);
    }
    drawText("SWEEPS " + std::to_string(m_State.players[p].sweeps),
             glm::vec2{offsetX + 120.f, 64.f}, 3.f,
             glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  }

  drawText("TURN P" + std::to_string(m_State.current + 1),
           glm::vec2{width * 0.5f - 60.f, 14.f}, 4.f,
           m_PlayerColors[m_State.current % m_PlayerColors.size()]);
}

void KasinoGame::drawHands() {
  float viewWidth = m_Camera.LogicalWidth();
  float viewHeight = m_Camera.LogicalHeight();
  auto inViewport = [&](const Rect &r) {
    return !(r.x > viewWidth || r.y > viewHeight || (r.x + r.w) < 0.f ||
             (r.y + r.h) < 0.f);
  };

  for (int p = 0; p < m_State.numPlayers; ++p) {
    const auto &hand = m_State.players[p].hand;
    const auto &rects = m_PlayerHandRects[p];
    const auto &layout = m_PlayerSeatLayouts[p];
    bool verticalSeat = layout.orientation == SeatOrientation::Vertical;
    float rotation = 0.f;
    if (verticalSeat) {
      bool isLeftSeat = layout.anchor.x < m_TableRect.x;
      rotation = isLeftSeat ? glm::half_pi<float>() : -glm::half_pi<float>();
    }

    for (size_t i = 0; i < hand.size(); ++i) {
      if (i >= rects.size()) {
        continue;
      }

      Rect drawRect = rects[i];
      if (verticalSeat) {
        glm::vec2 center = rects[i].Center();
        drawRect = {center.x - m_CardWidth * 0.5f,
                    center.y - m_CardHeight * 0.5f, m_CardWidth, m_CardHeight};
      }

      Rect viewportRect = verticalSeat ? rects[i] : drawRect;
      if (!inViewport(viewportRect)) {
        continue;
      }

      bool cardRevealed =
          (p < static_cast<int>(m_DealtCounts.size()) &&
           static_cast<int>(i) < m_DealtCounts[p]);

      const DealAnim *anim = nullptr;
      for (const auto &deal : m_DealQueue) {
        if (deal.player == p && deal.handIndex == static_cast<int>(i) &&
            deal.delay <= 0.f) {
          anim = &deal;
          break;
        }
      }

      if (!cardRevealed && !anim) {
        continue;
      }

      bool isCurrent = (p == m_State.current);
      bool isAI = (p < static_cast<int>(m_IsAiPlayer.size()) &&
                   m_IsAiPlayer[p]);
      bool isPendingCard =
          (cardRevealed && m_PendingAiMove && p == m_PendingAiPlayer &&
           m_PendingAiHandIndex == static_cast<int>(i));
      bool animatingCard =
          (isPendingCard && m_PendingAiMove && m_AiDecisionTimer <= 0.f &&
           m_AiAnimProgress > 0.f);

      auto drawOverlay = [&](const glm::vec4 &color) {
        if (verticalSeat) {
          Render2D::DrawQuad(buildCardTransform(drawRect, rotation), color);
        } else {
          Render2D::DrawQuad(glm::vec2{drawRect.x, drawRect.y},
                             glm::vec2{drawRect.w, drawRect.h}, color);
        }
      };

      if (anim) {
        float t = anim->progress;
        if (t < 0.f)
          t = 0.f;
        if (t > 1.f)
          t = 1.f;
        glm::vec2 targetCenter = drawRect.Center();
        glm::vec2 currentCenter = glm::mix(m_DeckOrigin, targetCenter, t);
        Rect animRect{currentCenter.x - m_CardWidth * 0.5f,
                      currentCenter.y - m_CardHeight * 0.5f, m_CardWidth,
                      m_CardHeight};
        if (verticalSeat) {
          if (isAI) {
            drawCardBack(animRect, isCurrent, rotation);
          } else {
            drawCardFace(anim->card, animRect, rotation, isCurrent, false,
                         false, false);
          }
        } else {
          if (isAI) {
            drawCardBack(animRect, isCurrent);
          } else {
            drawCardFace(anim->card, animRect, isCurrent, false, false,
                         false);
          }
        }
        continue;
      }

      if (isAI) {
        if (!animatingCard) {
          if (verticalSeat) {
            drawCardBack(drawRect, isCurrent, rotation);
          } else {
            drawCardBack(drawRect, isCurrent);
          }
          if (isPendingCard) {
            drawOverlay(glm::vec4(0.95f, 0.85f, 0.2f, 0.35f));
          }
        }
      } else {
        bool selected =
            (isCurrent && m_Selection.handIndex &&
             *m_Selection.handIndex == static_cast<int>(i));
        if (!animatingCard) {
          if (verticalSeat) {
            drawCardFace(hand[i], drawRect, rotation, isCurrent, selected,
                         false, false);
          } else {
            drawCardFace(hand[i], drawRect, isCurrent, selected, false,
                         false);
          }
          if (isPendingCard) {
            drawOverlay(glm::vec4(0.95f, 0.85f, 0.2f, 0.35f));
          }
        }
      }
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
    bool pending =
        i < m_PendingLooseHighlights.size() ? m_PendingLooseHighlights[i] : false;
    bool hovered = m_HoveredLoose.count(static_cast<int>(i)) != 0;
    drawCardFace(loose[i], m_LooseRects[i], true, selected, legal || pending,
                 hovered);
  }

  const auto &builds = m_State.table.builds;
  for (size_t i = 0; i < builds.size(); ++i) {
    bool selected = m_Selection.builds.count(static_cast<int>(i)) != 0;
    bool legal = i < m_BuildHighlights.size() ? m_BuildHighlights[i] : false;
    bool pending =
        i < m_PendingBuildHighlights.size() ? m_PendingBuildHighlights[i] : false;
    bool hovered = m_HoveredBuilds.count(static_cast<int>(i)) != 0;
    drawBuildFace(builds[i], m_BuildRects[i], legal || pending, hovered,
                  selected);
  }

  if (m_PendingAiMove && m_PendingAiPlayer >= 0 &&
      m_PendingAiHandIndex >= 0 && m_AiDecisionTimer <= 0.f &&
      m_AiAnimProgress > 0.f) {
    if (m_PendingAiPlayer < static_cast<int>(m_PlayerHandRects.size()) &&
        m_PendingAiHandIndex <
            static_cast<int>(m_PlayerHandRects[m_PendingAiPlayer].size())) {
      const Rect &startRect =
          m_PlayerHandRects[m_PendingAiPlayer][m_PendingAiHandIndex];
      glm::vec2 startCenter = startRect.Center();
      std::vector<glm::vec2> targetCenters;

      auto addLooseCenters = [&](const std::vector<int> &indices) {
        for (int idx : indices) {
          if (idx >= 0 && idx < static_cast<int>(m_LooseRects.size())) {
            targetCenters.push_back(m_LooseRects[idx].Center());
          }
        }
      };
      auto addBuildCenters = [&](const std::vector<int> &indices) {
        for (int idx : indices) {
          if (idx >= 0 && idx < static_cast<int>(m_BuildRects.size())) {
            targetCenters.push_back(m_BuildRects[idx].Center());
          }
        }
      };

      if (m_PendingAiMove->type == Casino::MoveType::Capture) {
        addLooseCenters(m_PendingAiMove->captureLooseIdx);
        addBuildCenters(m_PendingAiMove->captureBuildIdx);
      } else if (m_PendingAiMove->type == Casino::MoveType::Build) {
        addLooseCenters(m_PendingAiMove->buildUseLooseIdx);
      } else if (m_PendingAiMove->type == Casino::MoveType::ExtendBuild) {
        addLooseCenters(m_PendingAiMove->buildUseLooseIdx);
        addBuildCenters(m_PendingAiMove->captureBuildIdx);
      }

      if (targetCenters.empty()) {
        targetCenters.push_back(m_TableRect.Center());
      }

      glm::vec2 targetCenter(0.f);
      for (const auto &pt : targetCenters) targetCenter += pt;
      targetCenter /= static_cast<float>(targetCenters.size());

      float t = std::clamp(m_AiAnimProgress, 0.f, 1.f);
      glm::vec2 currentCenter = glm::mix(startCenter, targetCenter, t);
      Rect cardRect{currentCenter.x - m_CardWidth * 0.5f,
                    currentCenter.y - m_CardHeight * 0.5f, m_CardWidth,
                    m_CardHeight};
      drawCardFace(m_PendingAiMove->handCard, cardRect, false, false, false,
                   false);
    }
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
    drawTable();
    drawActionPanel();
    drawHands();
    drawScoreboard();
  }
  drawPromptOverlay();
}

void KasinoGame::drawMainMenu() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float centerX = width * 0.5f;

  glm::vec2 titleMetrics = measureText(kMainMenuTitleText, kMainMenuTitleScale);
  glm::vec2 subtitleMetrics =
      measureText(kMainMenuSubtitleText, kMainMenuSubtitleScale);
  glm::vec2 footerMetrics =
      measureText(kMainMenuFooterText, kMainMenuFooterScale);

  float titleY = height * 0.25f - titleMetrics.y;
  float titleToSubtitleSpacing = titleMetrics.y * kTitleSubtitleSpacingFactor;
  float subtitleY = titleY + titleMetrics.y + titleToSubtitleSpacing;
  float buttonsToFooterSpacing =
      footerMetrics.y * kButtonsFooterSpacingFactor;

  Render2D::DrawQuad(glm::vec2{0.f, 0.f}, glm::vec2{width, height},
                     glm::vec4(0.06f, 0.12f, 0.15f, 1.0f));

  drawText(kMainMenuTitleText,
           glm::vec2{centerX - titleMetrics.x * 0.5f, titleY},
           kMainMenuTitleScale,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  drawText(kMainMenuSubtitleText,
           glm::vec2{centerX - subtitleMetrics.x * 0.5f, subtitleY},
           kMainMenuSubtitleScale,
           glm::vec4(0.8f, 0.85f, 0.9f, 1.0f));

  auto drawButton = [&](const Rect &rect, const std::string &label,
                        bool hovered) {
    glm::vec4 baseColor = hovered ? glm::vec4(0.30f, 0.55f, 0.78f, 1.0f)
                                  : glm::vec4(0.18f, 0.32f, 0.38f, 1.0f);
    Render2D::DrawQuad(glm::vec2{rect.x - 4.f, rect.y - 4.f},
                       glm::vec2{rect.w + 8.f, rect.h + 8.f},
                       glm::vec4(0.03f, 0.05f, 0.06f, 1.0f));
    Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                       baseColor);
    glm::vec2 labelMetrics = measureText(label, 4.f);
    float textX = rect.x + rect.w * 0.5f - labelMetrics.x * 0.5f;
    float textY = rect.y + rect.h * 0.5f - labelMetrics.y * 0.5f;
    drawText(label, glm::vec2{textX, textY}, 4.f,
             glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  };

  drawButton(m_MainMenuStartButtonRect, "START", m_MainMenuStartHovered);
  drawButton(m_MainMenuSettingsButtonRect, "SETTINGS",
             m_MainMenuSettingsHovered);

  float footerY = m_MainMenuSettingsButtonRect.y +
                  m_MainMenuSettingsButtonRect.h + buttonsToFooterSpacing;
  drawText(kMainMenuFooterText,
           glm::vec2{centerX - footerMetrics.x * 0.5f, footerY},
           kMainMenuFooterScale,
           glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));
}

void KasinoGame::OnRender() { drawScene(); }

