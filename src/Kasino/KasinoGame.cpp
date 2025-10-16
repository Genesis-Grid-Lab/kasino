#include "Kasino/KasinoGame.h"

#include "app/Game.h"
#include "core/Factory.h"
#include "core/Log.h"
#include "gfx/Render2D.h"
#include "input/InputSystem.h"
#include "ui/UISystem.h"
#include "Kasino/GameLogic.h"
#include "Kasino/Scoring.h"
#include "audio/SoundSystem.h"

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
constexpr float kMainMenuBottomMargin = 48.f;

constexpr const char *kSettingsParagraph1 =
    "Close resumes play without leaving the table.";
constexpr const char *kSettingsParagraph2 =
    "Quit Game ends the current match and exits immediately.";
constexpr const char *kSettingsParagraph3 =
    "Press ESC or tap Settings to reopen this menu.";

const std::array<const char *, 20> kHowToPlayLines = {
    "EACH ROUND STARTS WITH FOUR CARDS PER PLAYER",
    "AND FOUR FACE UP ON THE TABLE",
    "ON YOUR TURN CHOOSE ONE ACTION:",
    " - CAPTURE MATCHING RANKS OR SUM TABLE CARDS",
    "   TO YOUR CARD VALUE AND TAKE BUILDS OF THAT VALUE",
    " - BUILD BY COMBINING YOUR CARD WITH TABLE CARDS",
    "   TO RESERVE A TARGET VALUE FOR LATER CAPTURE",
    " - EXTEND YOUR OWN BUILDS TO A HIGHER VALUE WHEN",
    "   YOU HOLD THE NEEDED CARD",
    " - TRAIL TO PLACE A CARD ON THE TABLE WHEN NO",
    "   OTHER MOVE FITS",
    "CAPTURED CARDS GO TO YOUR PILE AND CLEARING THE",
    "TABLE DURING A CAPTURE EARNS A SWEEP BONUS",
    "WHEN ALL HANDS ARE EMPTY THE LAST PLAYER TO CAPTURE",
    "TAKES ANY CARDS LEFT ON THE TABLE",
    "SCORING EACH ROUND:",
    " - ONE POINT PER CARD IN YOUR PILE",
    " - ONE POINT FOR EACH BUILD YOU COLLECT",
    " - ONE POINT FOR EVERY SWEEP BONUS",
    "REACH THE TARGET SCORE TO WIN THE MATCH"};

constexpr float kPromptTextStart = 64.f;
constexpr float kMainMenuSettingsOptionTop = 110.f;
constexpr float kMainMenuSettingsDescriptionTop = 180.f;
constexpr float kMainMenuSettingsOptionHeight = 44.f;
constexpr float kMainMenuSettingsOptionSpacing = 20.f;

float lineAdvanceForStyle(const ui::TextStyle &style) {
  return style.scale * (5.0f + style.lineSpacing);
}

float blockHeightForLines(const std::vector<std::string> &lines,
                          const ui::TextStyle &style) {
  if (lines.empty()) {
    return 0.f;
  }
  float baseHeight = style.scale * 5.0f;
  if (lines.size() == 1) {
    return baseHeight;
  }
  return baseHeight +
         static_cast<float>(lines.size() - 1) * lineAdvanceForStyle(style);
}

template <size_t N>
std::string joinLines(const std::array<const char *, N> &lines) {
  std::string result;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      result.push_back('\n');
    }
    result.append(lines[i]);
  }
  return result;
}

std::vector<std::string> wrapText(const std::string &text,
                                  const ui::TextStyle &style,
                                  float maxWidth) {
  std::vector<std::string> wrapped;
  if (text.empty()) {
    return wrapped;
  }

  std::stringstream ss(text);
  std::string paragraph;
  bool processedAny = false;
  while (std::getline(ss, paragraph)) {
    processedAny = true;
    if (paragraph.empty()) {
      wrapped.emplace_back();
      continue;
    }

    std::istringstream words(paragraph);
    std::string word;
    std::string currentLine;
    while (words >> word) {
      std::string candidate =
          currentLine.empty() ? word : currentLine + " " + word;
      float width = ui::MeasureText(candidate, style).x;
      if (maxWidth <= 0.f || width <= maxWidth) {
        currentLine = std::move(candidate);
      } else {
        if (!currentLine.empty()) {
          wrapped.push_back(currentLine);
        }
        currentLine = word;
      }
    }

    if (!currentLine.empty()) {
      wrapped.push_back(currentLine);
    } else if (wrapped.empty() || !wrapped.back().empty()) {
      wrapped.emplace_back();
    }
  }

  if (!processedAny) {
    return wrapped;
  }

  while (!wrapped.empty() && wrapped.back().empty()) {
    wrapped.pop_back();
  }
  return wrapped;
}

float drawWrappedLines(const std::vector<std::string> &lines, glm::vec2 pos,
                       const ui::TextStyle &style) {
  if (lines.empty()) {
    return 0.f;
  }

  float y = pos.y;
  float advance = lineAdvanceForStyle(style);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!lines[i].empty()) {
      ui::DrawText(lines[i], glm::vec2{pos.x, y}, style);
    }
    if (i + 1 < lines.size()) {
      y += advance;
    }
  }

  return blockHeightForLines(lines, style);
}

struct PreviewScoreResult {
  std::vector<ScoreLine> lines;
};

PreviewScoreResult computePreviewScores(const GameState &state) {
  PreviewScoreResult result;
  GameState previewState = state;
  result.lines = ScoreRound(previewState);
  return result;
}

}  // namespace
glm::mat4 KasinoGame::buildCardTransform(const Rect &rect, float rotation) {
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

bool KasinoGame::OnStart() {
  if (!Render2D::Initialize()) {
    EN_ERROR("Render2D initialization failed");
    return false;
  }

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
  m_PromptButtonLabel.clear();
  m_PromptSecondaryButtonLabel.clear();
  m_MenuDifficulty = Difficulty::Easy;
  m_ActiveDifficulty = m_MenuDifficulty;

  m_GlobAudioSource = SoundSystem::GetDevice()->CreateSource();
  m_Audio_1 = SoundSystem::GetDevice()->CreateBuffer();

  if(!m_Audio_1->LoadWavFile("Resources/audio_1.wav")){
    EN_CORE_ERROR("Failed to load wav file");
  }

  auto loadBuffer = [](Ref<IAudioBuffer> &buffer, const std::string &path) {
    buffer = SoundSystem::GetDevice()->CreateBuffer();
    if (!buffer) {
      EN_CORE_ERROR("Failed to create audio buffer for {}", path);
      return;
    }
    if (!buffer->LoadWavFile(path.c_str())) {
      EN_CORE_ERROR("Failed to load wav file: {}", path);
      buffer.reset();
    }
  };
  
  loadBuffer(m_card_slide_1, "Resources/audio/card_slide_1.wav");
  loadBuffer(m_card_slide_2, "Resources/audio/card_slide_2.wav");
  loadBuffer(m_sndBuild, "Resources/audio/build.wav");
  loadBuffer(m_sndTrail, "Resources/audio/trail.wav");
  loadBuffer(m_sndTake, "Resources/audio/take.wav");
  loadBuffer(m_sndSweep, "Resources/audio/sweep.wav");
  loadBuffer(m_sndWin, "Resources/audio/win.wav");
  loadBuffer(m_sndRoundEnd, "Resources/audio/round_end.wav");
  loadBuffer(m_sndNewGame, "Resources/audio/new_game.wav");

  return true;
}

void KasinoGame::loadCardTextures() {
  m_CardTextures.clear();

  const std::array<Suit, 4> suits = {Suit::Clubs,
				     Suit::Diamonds,
				     Suit::Hearts,
				     Suit::Spades};

  for (Suit suit : suits) {
    for (int value = RankValue(Rank::Ace);
         value <= RankValue(Rank::King); ++value) {
      Rank rank = static_cast<Rank>(value);
      Card card(rank, suit);
      std::string key = cardTextureKey(card);
      std::string path = cardTexturePath(card);

      auto texture = Factory::CreateTexture2D();
      if (!texture) {
        EN_ERROR("Failed to create texture for card {} from {}", key, path);
        continue;
      }

      if (texture->LoadFromFile(path.c_str(), false)) {
        m_CardTextures[key] = texture;
      } else {
        EN_ERROR("Failed to load texture for card {} from {}", key, path);
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
    EN_ERROR("Failed to load card back texture: {}", backPath);
  }
}

std::string KasinoGame::cardTextureKey(const Card &card) const {
  return card.ToString();
}

std::string KasinoGame::cardTexturePath(const Card &card) const {
  std::string folder = cardSuitFolder(card.suit);
  return "Resources/Cards/Standard/rect_cards/individual/" + folder + "/" +
         cardRankString(card.rank) + folder + ".png";
}

std::string KasinoGame::cardRankString(Rank rank) const {
  switch (rank) {
  case Rank::Ace:
    return "A";
  case Rank::Jack:
    return "J";
  case Rank::Queen:
    return "Q";
  case Rank::King:
    return "K";
  default:
    return std::to_string(RankValue(rank));
  }
}

std::string KasinoGame::cardSuitFolder(Suit suit) const {
  switch (suit) {
  case Suit::Clubs:
    return "club";
  case Suit::Diamonds:
    return "diamond";
  case Suit::Hearts:
    return "heart";
  case Suit::Spades:
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
  StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = LegalMoves(m_State);
  m_Selection.Clear();
  m_LastRoundScores.clear();
  m_PendingMove.reset();
  m_PendingLooseHighlights.clear();
  m_PendingBuildHighlights.clear();
  m_Phase = Phase::Playing;
  m_ShowPrompt = false;
  m_PromptMode = PromptMode::None;
  m_PromptButtonLabel.clear();
  m_PromptSecondaryButtonLabel.clear();
  updateRoundScorePreview();
  updateActionOptions();
  updateLayout();
  beginDealAnimation();
  PlayEventSound(m_sndNewGame);
}

void KasinoGame::startNextRound() {
  ++m_RoundNumber;
  StartRound(m_State, m_State.numPlayers, m_Rng());
  m_LegalMoves = LegalMoves(m_State);
  m_Selection.Clear();
  m_LastRoundScores.clear();
  m_PendingMove.reset();
  m_PendingLooseHighlights.clear();
  m_PendingBuildHighlights.clear();
  m_Phase = Phase::Playing;
  m_ShowPrompt = false;
  m_PromptMode = PromptMode::None;
  m_PromptButtonLabel.clear();
  m_PromptSecondaryButtonLabel.clear();
  updateRoundScorePreview();
  updateActionOptions();
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
    updateActionOptions();
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
  m_LegalMoves = LegalMoves(m_State);
  if (m_Selection.handIndex) {
    // Ensure the selected index is still valid; otherwise clear selection.
    if (m_State.current < 0 || m_State.current >= m_State.numPlayers) {
      m_Selection.Clear();
    } else {
      auto &hand = m_State.players[m_State.current].hand;
      if (*m_Selection.handIndex < 0 ||
          *m_Selection.handIndex >= static_cast<int>(hand.size())) {
        m_Selection.Clear();
      }
    }
  }
  updateActionOptions();
}

void KasinoGame::updateMainMenuLayout() {
  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  glm::vec2 titleMetrics = ui::MeasureText(kMainMenuTitleText, kMainMenuTitleScale);
  glm::vec2 subtitleMetrics =
      ui::MeasureText(kMainMenuSubtitleText, kMainMenuSubtitleScale);
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

  float maxButtonBottom = height - kMainMenuBottomMargin;
  float totalButtonsHeight = buttonHeight * 3.f + buttonSpacing * 2.f;
  if (startY + totalButtonsHeight > maxButtonBottom) {
    float allowedSpacing =
        (maxButtonBottom - startY - buttonHeight * 3.f) * 0.5f;
    if (allowedSpacing < 0.f) {
      allowedSpacing = 0.f;
    }
    buttonSpacing = std::min(buttonSpacing, allowedSpacing);

    totalButtonsHeight = buttonHeight * 3.f + buttonSpacing * 2.f;
    float maxStartY = maxButtonBottom - totalButtonsHeight;
    startY = std::min(startY, maxStartY);
  }

  m_MainMenuStartButtonRect = {startX, startY, buttonWidth, buttonHeight};
  m_MainMenuSettingsButtonRect = {startX, startY + buttonHeight + buttonSpacing,
                                  buttonWidth, buttonHeight};
  m_MainMenuHowToButtonRect =
      {startX,
       startY + (buttonHeight + buttonSpacing) * 2.f,
       buttonWidth, buttonHeight};

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

  const float settingsButtonSize = 48.f;
  const float settingsButtonPadding = 16.f;
  float settingsX = width - settingsButtonSize - settingsButtonPadding;
  float settingsY = m_ScoreboardHeight * 0.5f - settingsButtonSize * 0.5f;
  settingsX = std::max(settingsButtonPadding, settingsX);
  settingsY = std::clamp(settingsY, settingsButtonPadding,
                         m_ScoreboardHeight - settingsButtonSize -
                             settingsButtonPadding);
  m_SettingsButtonRect =
      {settingsX, settingsY, settingsButtonSize, settingsButtonSize};

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
  float confirmHeight = 32.f;
  float confirmSpacing = 8.f;
  m_ConfirmButtonRect = {m_ActionPanelRect.x + 12.f,
                         m_CancelButtonRect.y - confirmHeight - confirmSpacing,
                         m_ActionPanelRect.w - 24.f, confirmHeight};

  updatePromptLayout();
}

void KasinoGame::updatePromptLayout() {
  if (!m_ShowPrompt) {
    m_MenuPlayerCountRects.clear();
    m_MenuSeatToggleRects.clear();
    m_MenuSummaryTextY = 0.f;
    m_MenuInstructionTextY = 0.f;
    m_PromptButtonRect = {};
    m_PromptSecondaryButtonRect = {};
    return;
  }

  float width = m_Camera.LogicalWidth();
  float height = m_Camera.LogicalHeight();
  float boxWidth = width * 0.75f;
  float boxHeight = 220.f;
  const float buttonWidth = 180.f;
  const float buttonHeight = 40.f;
  const float buttonBottomPadding = 16.f;
  const float summaryMargin = 24.f;
  const float textSpacing = 24.f;
  const float buttonSpacing = 24.f;
  const float buttonHorizontalSpacing = 24.f;
  const float seatSpacing = 12.f;
  const float seatHeight = 32.f;
  const float optionHeight = 40.f;
  const float optionSpacing = 16.f;
  const float optionYBase = 90.f;
  const float seatHeaderSpacing = 60.f;

  m_PromptSecondaryButtonRect = {};

  switch (m_PromptMode) {
  case PromptMode::RoundSummary:
    boxHeight = 260.f;
    break;
  case PromptMode::MatchSummary:
    boxHeight = 220.f;
    break;
  case PromptMode::HowToPlay: {
    boxHeight = 420.f;
    ui::TextStyle style;
    style.scale = 2.6f;
    float textMaxWidth = boxWidth - 32.f;
    auto howLines = wrapText(joinLines(kHowToPlayLines), style, textMaxWidth);
    float textHeight = blockHeightForLines(howLines, style);
    float contentBottom = kPromptTextStart + textHeight;
    float requiredHeight =
        contentBottom + buttonSpacing + buttonHeight + buttonBottomPadding;
    boxHeight = std::max(boxHeight, requiredHeight);
  } break;
  case PromptMode::PlayerSetup: {
    boxHeight = 320.f;
    float seatStartOffset = optionYBase + optionHeight + seatHeaderSpacing;
    int seatCount = std::max(0, m_MenuSelectedPlayers);
    float seatBottomOffset = seatStartOffset;
    if (seatCount > 0) {
      seatBottomOffset += seatHeight * static_cast<float>(seatCount);
      seatBottomOffset += seatSpacing * static_cast<float>(seatCount - 1);
    }
    float summaryOffset = seatBottomOffset + summaryMargin;
    float instructionOffset = summaryOffset + textSpacing;
    float buttonTopOffset = instructionOffset + buttonSpacing;
    float requiredHeight =
        buttonTopOffset + buttonHeight + buttonBottomPadding;
    boxHeight = std::max(boxHeight, requiredHeight);
  } break;
  case PromptMode::Settings: {
    boxHeight = 240.f;
    float textMaxWidth = boxWidth - 32.f;
    const float paragraphSpacing = 14.f;
    ui::TextStyle primary;
    primary.scale = 3.2f;
    ui::TextStyle secondary;
    secondary.scale = 3.f;
    float totalTextHeight = 0.f;
    totalTextHeight += blockHeightForLines(
        wrapText(kSettingsParagraph1, primary, textMaxWidth), primary);
    totalTextHeight += paragraphSpacing;
    totalTextHeight += blockHeightForLines(
        wrapText(kSettingsParagraph2, secondary, textMaxWidth), secondary);
    totalTextHeight += paragraphSpacing;
    totalTextHeight += blockHeightForLines(
        wrapText(kSettingsParagraph3, secondary, textMaxWidth), secondary);
    float contentBottom = kPromptTextStart + totalTextHeight;
    float requiredHeight =
        contentBottom + buttonSpacing + buttonHeight + buttonBottomPadding;
    boxHeight = std::max(boxHeight, requiredHeight);
  } break;
  case PromptMode::MainMenuSettings: {
    boxHeight = 260.f;
    ui::TextStyle descStyle;
    descStyle.scale = 3.f;
    float textMaxWidth = boxWidth - 32.f;
    auto descLines = wrapText(difficultyDescription(m_MenuDifficulty), descStyle,
                              textMaxWidth);
    float descHeight = blockHeightForLines(descLines, descStyle);
    float optionBottom =
        kMainMenuSettingsOptionTop + kMainMenuSettingsOptionHeight;
    float descBottom =
        kMainMenuSettingsDescriptionTop + descHeight;
    float contentBottom = std::max(optionBottom, descBottom);
    float requiredHeight =
        contentBottom + buttonSpacing + buttonHeight + buttonBottomPadding;
    boxHeight = std::max(boxHeight, requiredHeight);
  } break;
  case PromptMode::None:
    break;
  }

  m_PromptBoxRect = {width * 0.5f - boxWidth * 0.5f,
                     height * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight};

  auto assignButtons = [&](float buttonTop) {
    if (!m_PromptSecondaryButtonLabel.empty()) {
      float totalWidth = buttonWidth * 2.f + buttonHorizontalSpacing;
      float startX = m_PromptBoxRect.x + (boxWidth - totalWidth) * 0.5f;
      m_PromptButtonRect = {startX, buttonTop, buttonWidth, buttonHeight};
      m_PromptSecondaryButtonRect =
          {startX + buttonWidth + buttonHorizontalSpacing, buttonTop,
           buttonWidth, buttonHeight};
    } else {
      m_PromptButtonRect =
          {m_PromptBoxRect.x + (boxWidth - buttonWidth) * 0.5f, buttonTop,
           buttonWidth, buttonHeight};
      m_PromptSecondaryButtonRect = {};
    }
  };

  if (m_PromptMode == PromptMode::PlayerSetup) {
    m_MenuPlayerCountRects.clear();
    m_MenuPlayerCountRects.reserve(4);
    float optionWidth = 60.f;
    float totalWidth = optionWidth * 4.f + optionSpacing * 3.f;
    float startX = m_PromptBoxRect.x + (boxWidth - totalWidth) * 0.5f;
    float optionY = m_PromptBoxRect.y + optionYBase;
    for (int i = 0; i < 4; ++i) {
      m_MenuPlayerCountRects.push_back(
          {startX + (float)i * (optionWidth + optionSpacing), optionY,
           optionWidth, optionHeight});
    }

    m_MenuSeatToggleRects.clear();
    float seatWidth = boxWidth - 48.f;
    float seatY = optionY + optionHeight + seatHeaderSpacing;
    for (int i = 0; i < m_MenuSelectedPlayers; ++i) {
      m_MenuSeatToggleRects.push_back(
          {m_PromptBoxRect.x + 24.f, seatY, seatWidth, seatHeight});
      seatY += seatHeight + seatSpacing;
    }

    float seatBottom =
        (m_MenuSelectedPlayers > 0) ? (seatY - seatSpacing) : seatY;
    m_MenuSummaryTextY = seatBottom + summaryMargin;
    m_MenuInstructionTextY = m_MenuSummaryTextY + textSpacing;
    float buttonTop = m_MenuInstructionTextY + buttonSpacing;
    assignButtons(buttonTop);
    m_DifficultyOptionRects.clear();
  } else if (m_PromptMode == PromptMode::MainMenuSettings) {
    m_MenuPlayerCountRects.clear();
    m_MenuSeatToggleRects.clear();
    m_MenuSummaryTextY = 0.f;
    m_MenuInstructionTextY = 0.f;
    m_DifficultyOptionRects.clear();
    float optionWidth = 120.f;
    float totalWidth =
        optionWidth * 3.f + kMainMenuSettingsOptionSpacing * 2.f;
    float startX = m_PromptBoxRect.x + (boxWidth - totalWidth) * 0.5f;
    float optionY = m_PromptBoxRect.y + kMainMenuSettingsOptionTop;
    for (int i = 0; i < 3; ++i) {
      m_DifficultyOptionRects.push_back(
          {startX + (float)i * (optionWidth + kMainMenuSettingsOptionSpacing),
           optionY, optionWidth, kMainMenuSettingsOptionHeight});
    }
    assignButtons(m_PromptBoxRect.y + boxHeight -
                  (buttonHeight + buttonBottomPadding));
  } else {
    m_MenuPlayerCountRects.clear();
    m_MenuSeatToggleRects.clear();
    m_MenuSummaryTextY = 0.f;
    m_MenuInstructionTextY = 0.f;
    m_DifficultyOptionRects.clear();
    assignButtons(m_PromptBoxRect.y + boxHeight -
                  (buttonHeight + buttonBottomPadding));
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
  if (m_ActiveDifficulty != Difficulty::Easy) {
    y += 20.f;
  }
  float w = m_ActionPanelRect.w - 24.f;
  float bottomLimit = m_ActionPanelRect.y + m_ActionPanelRect.h - 12.f;
  if (m_ActiveDifficulty != Difficulty::Easy && m_ConfirmButtonRect.h > 0.f) {
    bottomLimit = m_ConfirmButtonRect.y - 8.f;
  } else if (m_CancelButtonRect.h > 0.f) {
    bottomLimit = m_CancelButtonRect.y - 8.f;
  }
  for (auto &entry : m_ActionEntries) {
    if (y + buttonHeight > bottomLimit) break;
    entry.rect = {x, y, w, buttonHeight};
    y += buttonHeight + 8.f;
  }
}

void KasinoGame::refreshHighlights() {
  m_LooseHighlights.assign(m_LooseRects.size(), false);
  m_BuildHighlights.assign(m_BuildRects.size(), false);

  if (!m_Selection.handIndex) return;
  if (m_ActiveDifficulty == Difficulty::Hard) return;

  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard ==
          m_State.players[m_State.current].hand[*m_Selection.handIndex]))
      continue;
    if (!selectionCompatible(mv)) continue;

    if (mv.type == MoveType::Capture) {
      for (int idx : mv.captureLooseIdx)
        if (idx >= 0 && idx < (int)m_LooseHighlights.size())
          m_LooseHighlights[idx] = true;
      for (int idx : mv.captureBuildIdx)
        if (idx >= 0 && idx < (int)m_BuildHighlights.size())
          m_BuildHighlights[idx] = true;
    } else if (mv.type == MoveType::Build) {
      for (int idx : mv.buildUseLooseIdx)
        if (idx >= 0 && idx < (int)m_LooseHighlights.size())
          m_LooseHighlights[idx] = true;
    } else if (mv.type == MoveType::ExtendBuild) {
      for (int idx : mv.captureBuildIdx)
        if (idx >= 0 && idx < (int)m_BuildHighlights.size())
          m_BuildHighlights[idx] = true;
    }
  }
}

bool KasinoGame::selectionCompatible(const Move &mv) const {
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
  case MoveType::Capture:
    return subset(m_Selection.loose, mv.captureLooseIdx) &&
           subset(m_Selection.builds, mv.captureBuildIdx);
  case MoveType::Build:
    return m_Selection.builds.empty() &&
           subset(m_Selection.loose, mv.buildUseLooseIdx);
  case MoveType::ExtendBuild:
    return m_Selection.loose.empty() &&
           subset(m_Selection.builds, mv.captureBuildIdx);
  case MoveType::Trail:
    return m_Selection.loose.empty() && m_Selection.builds.empty();
  }
  return false;
}

std::string KasinoGame::moveLabel(const Move &mv) const {
  std::ostringstream oss;
  switch (mv.type) {
  case MoveType::Capture: {
    oss << "CAPTURE";
    for (int idx : mv.captureLooseIdx) {
      if (idx >= 0 && idx < (int)m_State.table.loose.size())
        oss << ' ' << m_State.table.loose[idx].ToString();
    }
    for (int idx : mv.captureBuildIdx) {
      oss << " B" << (idx + 1);
    }
  } break;
  case MoveType::Build: {
    oss << "BUILD TO " << mv.buildTargetValue;
    for (int idx : mv.buildUseLooseIdx) {
      if (idx >= 0 && idx < (int)m_State.table.loose.size())
        oss << ' ' << m_State.table.loose[idx].ToString();
    }
  } break;
  case MoveType::ExtendBuild: {
    oss << "RAISE TO " << mv.buildTargetValue;
    for (int idx : mv.captureBuildIdx) {
      oss << " B" << (idx + 1);
    }
  } break;
  case MoveType::Trail:
    oss << "TRAIL";
    break;
  }
  return oss.str();
}

std::string KasinoGame::moveLabelForDifficulty(const Move &mv,
                                               Difficulty difficulty) const {
  if (difficulty == Difficulty::Easy) {
    return moveLabel(mv);
  }

  std::ostringstream oss;
  switch (mv.type) {
  case MoveType::Capture: {
    oss << "CAPTURE";
    int targets = static_cast<int>(mv.captureLooseIdx.size()) +
                  static_cast<int>(mv.captureBuildIdx.size());
    if (targets > 0) {
      oss << " (" << targets << (targets == 1 ? " CARD" : " CARDS") << ")";
    }
  } break;
  case MoveType::Build:
    oss << "BUILD TO " << mv.buildTargetValue;
    break;
  case MoveType::ExtendBuild:
    oss << "RAISE BUILD TO " << mv.buildTargetValue;
    break;
  case MoveType::Trail:
    oss << "TRAIL";
    break;
  }
  return oss.str();
}

std::string KasinoGame::difficultyLabel(Difficulty difficulty) const {
  switch (difficulty) {
  case Difficulty::Easy:
    return "EASY";
  case Difficulty::Medium:
    return "MEDIUM";
  case Difficulty::Hard:
    return "HARD";
  }
  return "";
}

std::string KasinoGame::difficultyDescription(Difficulty difficulty) const {
  switch (difficulty) {
  case Difficulty::Easy:
    return "Shows full move details and highlights to guide play.";
  case Difficulty::Medium:
    return "Shows move types but fewer specifics—some planning required.";
  case Difficulty::Hard:
    return "No move hints. Select exact targets then confirm the play.";
  }
  return "";
}

bool KasinoGame::selectionMatches(const Move &mv) const {
  if (!m_Selection.handIndex) return false;

  auto vectorToSet = [](const std::vector<int> &indices) {
    return std::set<int>(indices.begin(), indices.end());
  };

  switch (mv.type) {
  case MoveType::Capture:
    return m_Selection.loose == vectorToSet(mv.captureLooseIdx) &&
           m_Selection.builds == vectorToSet(mv.captureBuildIdx);
  case MoveType::Build:
    return m_Selection.builds.empty() &&
           m_Selection.loose == vectorToSet(mv.buildUseLooseIdx);
  case MoveType::ExtendBuild:
    return m_Selection.loose.empty() &&
           m_Selection.builds == vectorToSet(mv.captureBuildIdx);
  case MoveType::Trail:
    return m_Selection.loose.empty() && m_Selection.builds.empty();
  }
  return false;
}

bool KasinoGame::movesEquivalent(const Move &a, const Move &b) const {
  if (a.type != b.type) return false;
  if (!(a.handCard == b.handCard)) return false;
  switch (a.type) {
  case MoveType::Capture:
    return a.captureLooseIdx == b.captureLooseIdx &&
           a.captureBuildIdx == b.captureBuildIdx;
  case MoveType::Build:
    return a.buildTargetValue == b.buildTargetValue &&
           a.buildUseLooseIdx == b.buildUseLooseIdx;
  case MoveType::ExtendBuild:
    return a.buildTargetValue == b.buildTargetValue &&
           a.captureBuildIdx == b.captureBuildIdx;
  case MoveType::Trail:
    return true;
  }
  return false;
}

void KasinoGame::updateActionOptions() {
  m_ActionEntries.clear();
  m_ConfirmableMove.reset();
  m_ConfirmAmbiguous = false;

  if (!m_Selection.handIndex) {
    layoutActionEntries();
    refreshHighlights();
    return;
  }

  if (m_State.current < 0 || m_State.current >= m_State.numPlayers) {
    layoutActionEntries();
    refreshHighlights();
    return;
  }

  const auto &players = m_State.players;
  if (m_State.current >= static_cast<int>(players.size())) {
    layoutActionEntries();
    refreshHighlights();
    return;
  }

  const auto &hand = players[m_State.current].hand;
  int handIndex = *m_Selection.handIndex;
  if (handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
    layoutActionEntries();
    refreshHighlights();
    return;
  }

  for (const auto &mv : m_LegalMoves) {
    if (!(mv.handCard == hand[handIndex])) continue;
    if (!selectionCompatible(mv)) continue;

    bool exact = selectionMatches(mv);
    if (m_ActiveDifficulty != Difficulty::Hard) {
      ActionEntry entry;
      entry.move = mv;
      entry.label = moveLabelForDifficulty(mv, m_ActiveDifficulty);
      m_ActionEntries.push_back(entry);
    }

    if (exact) {
      if (!m_ConfirmableMove) {
        m_ConfirmableMove = mv;
      } else if (!movesEquivalent(*m_ConfirmableMove, mv)) {
        m_ConfirmableMove.reset();
        m_ConfirmAmbiguous = true;
      }
    }
  }

  if (m_ConfirmAmbiguous) {
    m_ConfirmableMove.reset();
  }

  layoutActionEntries();
  refreshHighlights();
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
    updateActionOptions();
    return;
  }

  m_Selection.handIndex = index;
  m_Selection.loose.clear();
  m_Selection.builds.clear();
  updateActionOptions();
}

void KasinoGame::toggleLooseCard(int idx) {
  if (!m_Selection.handIndex) return;
  if (m_Selection.loose.count(idx))
    m_Selection.loose.erase(idx);
  else
    m_Selection.loose.insert(idx);

  updateActionOptions();
}

void KasinoGame::toggleBuild(int idx) {
  if (!m_Selection.handIndex) return;
  if (m_Selection.builds.count(idx))
    m_Selection.builds.erase(idx);
  else
    m_Selection.builds.insert(idx);

  updateActionOptions();
}

void KasinoGame::processInput(float mx, float my) {
  if (!m_Input) return;
  bool click = m_Input->WasMousePressed(MouseButton::Left);
  if (!click) return;

  if (m_Phase != Phase::Playing)
    return;

  if (m_IsDealing)
    return;

  if (m_PendingMove)
    return;

  if (m_State.current >= 0 &&
      m_State.current < static_cast<int>(m_IsAiPlayer.size()) &&
      m_IsAiPlayer[m_State.current]) {
    return;
  }

  if (m_ActiveDifficulty != Difficulty::Easy && m_ConfirmableMove &&
      m_ConfirmButtonRect.Contains(mx, my)) {
    int handIndex = m_Selection.handIndex ? *m_Selection.handIndex : -1;
    if (handIndex >= 0) {
      beginPendingMove(*m_ConfirmableMove, m_State.current, handIndex, 0.f);
      m_Selection.Clear();
      updateActionOptions();
    }
    return;
  }

  // Action entries
  for (size_t i = 0; i < m_ActionEntries.size(); ++i) {
    if (m_ActionEntries[i].rect.Contains(mx, my)) {
      int handIndex = m_Selection.handIndex ? *m_Selection.handIndex : -1;
      beginPendingMove(m_ActionEntries[i].move, m_State.current, handIndex, 0.f);
      m_Selection.Clear();
      updateActionOptions();
      return;
    }
  }

  if (m_Selection.handIndex && m_CancelButtonRect.Contains(mx, my)) {
    m_Selection.Clear();
    updateActionOptions();
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
  updateActionOptions();
}

bool KasinoGame::playAiTurn() {
  if (m_Phase != Phase::Playing) return false;
  if (m_State.RoundOver()) return false;
  if (m_State.current < 0 || m_State.current >= m_State.numPlayers) return false;
  if (m_State.current >= static_cast<int>(m_IsAiPlayer.size())) return false;
  if (!m_IsAiPlayer[m_State.current]) return false;
  if (m_PendingMove) return false;

  if (m_LegalMoves.empty()) {
    updateLegalMoves();
  }
  if (m_LegalMoves.empty()) return false;

  const Move *selected = nullptr;
  const Move *trailMove = nullptr;
  for (const auto &mv : m_LegalMoves) {
    if (mv.type == MoveType::Capture) {
      selected = &mv;
      break;
    }
    if (!trailMove && mv.type == MoveType::Trail) {
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

  Move chosen = *selected;
  beginPendingMove(chosen, m_State.current, -1, kAiDecisionDelay);
  return true;
}

void KasinoGame::beginPendingMove(const Move &mv, int player,
                                  int handIndex, float delay) {
  PendingMove pending;
  pending.move = mv;
  pending.player = player;
  pending.handIndex = handIndex;
  pending.delay = std::max(0.f, delay);
  pending.progress = 0.f;

  if (pending.player >= 0 &&
      pending.player < static_cast<int>(m_State.players.size())) {
    const auto &hand = m_State.players[pending.player].hand;
    if (pending.handIndex < 0 || pending.handIndex >= static_cast<int>(hand.size())) {
      pending.handIndex = -1;
      for (size_t i = 0; i < hand.size(); ++i) {
        if (hand[i] == mv.handCard) {
          pending.handIndex = static_cast<int>(i);
          break;
        }
      }
    }
  }

  m_PendingMove = pending;

  m_PendingLooseHighlights.assign(m_LooseRects.size(), false);
  m_PendingBuildHighlights.assign(m_BuildRects.size(), false);

  auto markHighlights = [](const std::vector<int> &indices,
                           std::vector<bool> &flags) {
    for (int idx : indices) {
      if (idx >= 0 && idx < static_cast<int>(flags.size())) {
        flags[idx] = true;
      }
    }
  };

  if (mv.type == MoveType::Capture) {
    markHighlights(mv.captureLooseIdx, m_PendingLooseHighlights);
    markHighlights(mv.captureBuildIdx, m_PendingBuildHighlights);
  } else if (mv.type == MoveType::Build) {
    markHighlights(mv.buildUseLooseIdx, m_PendingLooseHighlights);
  } else if (mv.type == MoveType::ExtendBuild) {
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
    m_PromptSecondaryButtonLabel.clear();
    updatePromptLayout();
  } else if (m_MainMenuSettingsButtonRect.Contains(mx, my)) {
    m_ShowPrompt = true;
    m_PromptMode = PromptMode::MainMenuSettings;
    m_PromptHeader = "GAME SETTINGS";
    m_PromptButtonLabel = "CLOSE";
    m_PromptSecondaryButtonLabel.clear();
    updatePromptLayout();
  } else if (m_MainMenuHowToButtonRect.Contains(mx, my)) {
    m_ShowPrompt = true;
    m_PromptMode = PromptMode::HowToPlay;
    m_PromptHeader = "HOW TO PLAY";
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
  } else if (m_PromptMode == PromptMode::MainMenuSettings) {
    if (click) {
      std::array<Difficulty, 3> difficulties = {
          Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
      for (size_t i = 0; i < m_DifficultyOptionRects.size() &&
                       i < difficulties.size();
           ++i) {
        if (m_DifficultyOptionRects[i].Contains(mx, my)) {
          Difficulty selected = difficulties[i];
          if (m_MenuDifficulty != selected) {
            m_MenuDifficulty = selected;
          }
          handled = true;
          break;
        }
      }
    }
  }

  bool primaryEnabled = true;
  if (m_PromptMode == PromptMode::PlayerSetup) {
    primaryEnabled = m_MenuSelectedHumans > 0 && m_MenuSelectedPlayers > 0;
  }

  if (click && !m_PromptButtonLabel.empty() && primaryEnabled &&
      m_PromptButtonRect.Contains(mx, my)) {
    handlePrompt(PromptAction::Primary);
    handled = true;
  }

  if (click && !m_PromptSecondaryButtonLabel.empty() &&
      m_PromptSecondaryButtonRect.Contains(mx, my)) {
    handlePrompt(PromptAction::Secondary);
    handled = true;
  }

  return handled;
}
 
void KasinoGame::applyMove(const Move &mv) {
  bool tableWasNotEmpty = !m_State.table.loose.empty() ||
                          !m_State.table.builds.empty();
  MoveType moveType = mv.type;

  if (!ApplyMove(m_State, mv)) return;

  bool sweep = moveType == MoveType::Capture && tableWasNotEmpty &&
               m_State.table.loose.empty() && m_State.table.builds.empty();

  switch (moveType) {
  case MoveType::Capture:
    PlayEventSound(m_sndTake);
    break;
  case MoveType::Build:
  case MoveType::ExtendBuild:
    PlayEventSound(m_sndBuild);
    break;
  case MoveType::Trail:
    PlayEventSound(m_sndTrail);
    break;
  }

  if (sweep) {
    PlayEventSound(m_sndSweep);
  }

  m_Selection.Clear();
  updateLegalMoves();
  updateLayout();

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
    m_PromptSecondaryButtonLabel.clear();
    updatePromptLayout();
  } else {
    updateRoundScorePreview();
  }
}

void KasinoGame::handleRoundEnd() {
  PlayEventSound(m_sndRoundEnd);
  m_LastRoundScores = ScoreRound(m_State);
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
                       PlayEventSound(m_sndWin);
    } else {
      m_PromptHeader = "MATCH COMPLETE";
    }
    m_PromptButtonLabel = "START NEW MATCH";
    m_PromptSecondaryButtonLabel.clear();
  } else {
    m_PromptHeader = "ROUND " + std::to_string(m_RoundNumber) + " COMPLETE";
    // m_PromptButtonLabel = "NEXT ROUND";
    m_PromptButtonLabel = "DEAL NEXT HAND";
    m_PromptSecondaryButtonLabel.clear();
  }
  updatePromptLayout();
}

void KasinoGame::handlePrompt(PromptAction action) {
  switch (m_PromptMode) {
  case PromptMode::MatchSummary:
    if (action == PromptAction::Primary) {
      m_MenuSelectedPlayers = m_State.numPlayers;
      for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(m_SeatIsAI.size())) {
          m_MenuSeatIsAI[i] = m_SeatIsAI[i];
        } else {
          m_MenuSeatIsAI[i] = true;
        }
      }
      m_MenuDifficulty = m_ActiveDifficulty;
      startNewMatch();
    }
    break;
  case PromptMode::RoundSummary:
    if (action == PromptAction::Primary) {
      startNextRound();
    }
    break;
  case PromptMode::HandSummary: {
    if (action != PromptAction::Primary) {
      break;
    }
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    m_PromptButtonLabel.clear();
    m_PromptSecondaryButtonLabel.clear();
    m_Phase = Phase::Playing; // tmpish
    if (!DealNextHands(m_State)) {
      handleRoundEnd();
      updateLayout();
      refreshHighlights();
      break;
    }
    // updateLegalMoves();
    m_LegalMoves = LegalMoves(m_State);
    m_Selection.Clear();
    updateActionOptions();
    updateLayout();
    // refreshHighlights();
    beginDealAnimation();
    updateRoundScorePreview();
    if (!m_IsDealing) {
      refreshHighlights();
    }
  } break;
  case PromptMode::PlayerSetup: {
    if (action != PromptAction::Primary) {
      break;
    }
    updateMenuHumanCounts();
    if (m_MenuSelectedHumans <= 0 || m_MenuSelectedPlayers <= 0) return;
    m_State.numPlayers = m_MenuSelectedPlayers;
    m_State.players.resize(m_State.numPlayers);
    m_SeatIsAI.assign(m_State.numPlayers, false);
    for (int i = 0; i < m_State.numPlayers && i < 4; ++i) {
      m_SeatIsAI[i] = m_MenuSeatIsAI[i];
    }
    m_HumanSeatCount = m_MenuSelectedHumans;
    m_ActiveDifficulty = m_MenuDifficulty;
    startNewMatch();
  } break;
  case PromptMode::Settings:
    if (action == PromptAction::Secondary) {
      Stop();
    }
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    m_PromptButtonLabel.clear();
    m_PromptSecondaryButtonLabel.clear();
    break;
  case PromptMode::MainMenuSettings:
    if (action == PromptAction::Primary) {
      m_ShowPrompt = false;
      m_PromptMode = PromptMode::None;
      m_PromptButtonLabel.clear();
      m_PromptSecondaryButtonLabel.clear();
    }
    break;
  case PromptMode::HowToPlay:
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    break;  
  case PromptMode::None:
    m_ShowPrompt = false;
    m_PromptMode = PromptMode::None;
    m_PromptButtonLabel.clear();
    m_PromptSecondaryButtonLabel.clear();
    break;
  }

  updatePromptLayout();
}

void KasinoGame::playCardSlideSound() {
  Ref<IAudioBuffer> buffer;

  if(m_GlobAudioSource->IsPlaying()){
        SoundSystem::Stop(m_GlobAudioSource);
      }
      // SoundSystem::Play(m_card_slide_1, m_GlobAudioSource);

  if (m_card_slide_1 && m_card_slide_2) {
    buffer = (m_NextCardSlideIndex == 0) ? m_card_slide_1 : m_card_slide_2;
    m_NextCardSlideIndex = 1 - m_NextCardSlideIndex;
  } else if (m_card_slide_1) {
    buffer = m_card_slide_1;
  } else if (m_card_slide_2) {
    buffer = m_card_slide_2;
  }

  if (buffer) {    
    SoundSystem::Play(buffer);
  }
}

void KasinoGame::PlayEventSound(const Ref<IAudioBuffer> &buffer) {
  if (buffer) {
    SoundSystem::Play(buffer);
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
      m_PromptButtonLabel.clear();
      m_PromptSecondaryButtonLabel.clear();
      updatePromptLayout();
    } else if (!m_ShowPrompt) {
      m_ShowPrompt = true;
      m_PromptMode = PromptMode::Settings;
      m_PromptHeader = "SETTINGS";
      m_PromptButtonLabel = "CLOSE";
      m_PromptSecondaryButtonLabel = "QUIT GAME";
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
      

      float previousProgress = anim.progress;
      if (kDealAnimDuration > 0.f) {
        anim.progress =
	  std::min(1.f, anim.progress + dt / kDealAnimDuration);
      } else {
        anim.progress = 1.f;
      }

      if(previousProgress <= 0.f && anim.progress > 0.f){
        playCardSlideSound();
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

  if (m_PendingMove) {
    if (m_PendingMove->delay > 0.f) {
      m_PendingMove->delay =
          std::max(0.f, m_PendingMove->delay - dt);
    } else {
      if (kAiAnimDuration > 0.f) {
        m_PendingMove->progress = std::min(
            1.f, m_PendingMove->progress + dt / kAiAnimDuration);
      } else {
        m_PendingMove->progress = 1.f;
      }

      if (m_PendingMove->progress >= 1.f) {
        Move move = m_PendingMove->move;
        m_PendingMove.reset();
        m_PendingLooseHighlights.clear();
        m_PendingBuildHighlights.clear();
        applyMove(move);
      }
    }
  }

  if (m_Phase == Phase::MainMenu) {
    SoundSystem::Play(m_Audio_1, m_GlobAudioSource, true);
    updateMainMenuLayout();
  } else {
    // SoundSystem::Stop(m_GlobAudioSource);
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
    if (aiTurn && !m_PendingMove) {
      playAiTurn();
    }
  }

  float mx = m_Input->MouseX();
  float my = m_Input->MouseY();
  bool mouseClick = m_Input->WasMousePressed(MouseButton::Left);
  m_LastMousePos = {mx, my};

  if (m_Phase == Phase::MainMenu) {
    m_MainMenuStartHovered = m_MainMenuStartButtonRect.Contains(mx, my);
    m_SettingsButtonHovered = false;
    m_MainMenuSettingsHovered = m_MainMenuSettingsButtonRect.Contains(mx, my);
    m_MainMenuHowToHovered = m_MainMenuHowToButtonRect.Contains(mx, my);
  } else {
    updateHoveredAction(mx, my);
    m_MainMenuStartHovered = false;
    m_MainMenuSettingsHovered = false;
    m_MainMenuHowToHovered = false;
    if (!m_ShowPrompt) {
      m_SettingsButtonHovered = m_SettingsButtonRect.Contains(mx, my);
      if (mouseClick && m_SettingsButtonHovered) {
        m_ShowPrompt = true;
        m_PromptMode = PromptMode::Settings;
        m_PromptHeader = "SETTINGS";
        m_PromptButtonLabel = "CLOSE";
        m_PromptSecondaryButtonLabel = "QUIT GAME";
        updatePromptLayout();
      }
    } else {
      m_SettingsButtonHovered = false;
    }
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

void KasinoGame::drawCardFace(const Card &card, const Rect &r,
                              bool isCurrent, bool selected, bool legal,
                              bool hovered) {
  drawCardFace(card, r, 0.f, isCurrent, selected, legal, hovered);
}

void KasinoGame::drawCardFace(const Card &card, const Rect &r,
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
      case Rank::Ace:
        return std::string("A");
      case Rank::Jack:
        return std::string("J");
      case Rank::Queen:
        return std::string("Q");
      case Rank::King:
        return std::string("K");
      default:
        return std::to_string(RankValue(card.rank));
      }
    }();

    auto suitString = [&]() {
      switch (card.suit) {
      case Suit::Clubs:
        return std::string("C");
      case Suit::Diamonds:
        return std::string("D");
      case Suit::Hearts:
        return std::string("H");
      case Suit::Spades:
        return std::string("S");
      }
      return std::string("?");
    }();

    glm::vec4 textColor =
        (card.suit == Suit::Hearts ||
         card.suit == Suit::Diamonds)
            ? glm::vec4(0.80f, 0.1f, 0.1f, 1.0f)
            : glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

    float scale = r.w / 10.f;
    ui::DrawText(rankString, glm::vec2{r.x + 6.f, r.y + 6.f}, scale, textColor);
    ui::DrawText(suitString,
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

void KasinoGame::drawBuildFace(const Build &build, const Rect &r,
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

  ui::DrawText("BUILD", glm::vec2{r.x + 6.f, r.y + 6.f}, r.w / 14.f,
           glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  ui::DrawText("VAL " + std::to_string(build.value),
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

    const bool settingsVisible =
        m_SettingsButtonRect.w > 0.f && m_SettingsButtonRect.h > 0.f;
    float leftBound = 16.f;
    float rightBound = settingsVisible ? (m_SettingsButtonRect.x - 6.f)
                                       : width - 16.f;
    if (rightBound < leftBound) rightBound = leftBound;

    constexpr float kMinColumnSpan = 160.f;
    float contentSpan = std::max(rightBound - leftBound, 0.f);
    float spanTarget = contentSpan;
    if (contentSpan > 0.f) {
        spanTarget = std::min(contentSpan, std::max(kMinColumnSpan,
                                                    std::min(m_TableRect.w, contentSpan)));
    }
    float hudCenter = m_TableRect.x + m_TableRect.w * 0.5f;
    float columnAreaLeft = leftBound;
    float columnAreaRight = leftBound + spanTarget;
    if (contentSpan > 0.f) {
        float minLeft = leftBound;
        float maxLeft = rightBound - spanTarget;
        if (maxLeft < minLeft) maxLeft = minLeft;
        columnAreaLeft =
            std::clamp(hudCenter - spanTarget * 0.5f, minLeft, maxLeft);
        columnAreaRight = columnAreaLeft + spanTarget;
    }
    float availableSpan = std::max(columnAreaRight - columnAreaLeft, 0.f);
    float constrainedSpan = std::max(availableSpan, kMinColumnSpan);
    float spanScale = constrainedSpan > 0.f ? (availableSpan / constrainedSpan) : 1.f;
    if (availableSpan <= 0.f) spanScale = 0.f;
    else if (spanScale <= 0.f) spanScale = 1.f;

    // ===== HEADER (ROUND / TURN / DECK) =====
    auto fitPx = [&](const std::string& s, float wantPx, float maxW) -> float {
        glm::vec2 m = ui::MeasureText(s, wantPx);
        if (m.x <= 0.0001f || maxW <= 0.f) return wantPx;
        if (m.x <= maxW) return wantPx;
        return wantPx * (maxW / m.x);
    };

    const float headerTop = 14.f;
    const float wantPx = 4.f;
    const float rowSpacing = 3.f;
    const float headerSpacing = 10.f;

    std::string roundLabel = "ROUND " + std::to_string(m_RoundNumber);
    std::string turnText   = "TURN P" + std::to_string(m_State.current + 1);
    std::string deckText   = "DECK " + std::to_string((int)m_State.stock.size());

    // 3 equal slots in available area
    float L0 = columnAreaLeft;
    float R1 = columnAreaRight;
    float slotW = std::max(0.f, (R1 - L0) / 3.f);
    float C0 = L0 + slotW;
    float C1 = L0 + 2.f * slotW;

    // Fit text into slots
    float pxL = fitPx(roundLabel, wantPx, slotW - 8.f);
    float pxC = fitPx(turnText,   wantPx, slotW - 8.f);
    float pxR = fitPx(deckText,   wantPx, slotW - 8.f);

    // Draw: left, center, right
    glm::vec2 roundPos{L0, headerTop};
    ui::DrawText(roundLabel, roundPos, pxL, glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));

    glm::vec2 turnMetrics = ui::MeasureText(turnText, pxC);
    float turnX = C0 + (slotW - turnMetrics.x) * 0.5f;
    ui::DrawText(turnText, glm::vec2{turnX, headerTop}, pxC,
                 m_PlayerColors[m_State.current % m_PlayerColors.size()]);

    glm::vec2 deckMetrics = ui::MeasureText(deckText, pxR);
    float deckX = C1 + (slotW - deckMetrics.x) * 0.5f;
    ui::DrawText(deckText, glm::vec2{deckX, headerTop}, pxR,
                 glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));

    float headerHeight = std::max({ ui::MeasureText(roundLabel, pxL).y,
                                    turnMetrics.y,
                                    deckMetrics.y });

    // ===== PLAYER GRID (SINGLE ROW) =====
    int playerCount = std::max(1, m_State.numPlayers);
    int cols = std::min(playerCount, 4);
    int rows = (playerCount + cols - 1) / cols; // should be 1

    const float pad = 12.f;
    float gridTop = headerTop + headerHeight + headerSpacing;
    float totalW = std::max(0.f, columnAreaRight - columnAreaLeft);
    float cellW = std::max(0.f, (totalW - pad * (cols + 1)) / cols);
    float cellH = 76.f;

    auto clampFitPx = [&](const std::string& s, float want, float maxWidth) -> float {
        glm::vec2 m = ui::MeasureText(s, want);
        if (m.x <= 0.0001f) return want;
        if (m.x <= maxWidth) return want;
        return want * (maxWidth / m.x);
    };

    for (int i = 0; i < playerCount; ++i) {
        int c = i % cols;
        float x0 = columnAreaLeft + pad + c * (cellW + pad);
        float y0 = gridTop;

        glm::vec4 color = m_PlayerColors[i % m_PlayerColors.size()];
        float innerX = x0 + 8.f;
        float curY = y0 + 10.f;
        float textMax = std::max(0.f, cellW - 16.f);

        std::string playerLabel = "PLAYER " + std::to_string(i + 1);
        float pxLabel = clampFitPx(playerLabel, 3.5f, textMax);
        ui::DrawText(playerLabel, glm::vec2{innerX, curY}, pxLabel, color);
        curY += ui::MeasureText(playerLabel, pxLabel).y + 3.f;

        int total = (i < (int)m_TotalScores.size()) ? m_TotalScores[i] : 0;
        const RunningScore* runningScore =
            (i < (int)m_CurrentRoundScores.size()) ? &m_CurrentRoundScores[i] : nullptr;

        if (m_Phase == Phase::Playing) {
            if (runningScore) {
                total += runningScore->line.total;
            } else if (i < (int)m_State.players.size()) {
                const auto& player = m_State.players[i];
                total += player.capturedCardPoints + player.buildBonus + player.sweepBonus;
            }
        }

        std::string totalText = "TOTAL " + std::to_string(total);
        float pxTotal = clampFitPx(totalText, 3.0f, textMax);
        ui::DrawText(totalText, glm::vec2{innerX, curY}, pxTotal,
                     glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
        curY += ui::MeasureText(totalText, pxTotal).y + 3.f;

        auto drawStat = [&](const std::string& label, int value) {
            std::string text = label + " +" + std::to_string(value);
            float px = clampFitPx(text, 2.6f, textMax);
            ui::DrawText(text, glm::vec2{innerX, curY}, px,
                         glm::vec4(0.9f, 0.94f, 0.92f, 1.0f));
            curY += ui::MeasureText(text, px).y;
        };

        int cardPoints = 0, buildBonus = 0, sweepBonus = 0;
        if (runningScore) {
            cardPoints = runningScore->line.capturedCardPoints;
            buildBonus = runningScore->line.buildBonus;
            sweepBonus = runningScore->line.sweepBonus;
        } else if (i < (int)m_State.players.size()) {
            const auto& player = m_State.players[i];
            cardPoints = player.capturedCardPoints;
            buildBonus = player.buildBonus;
            sweepBonus = player.sweepBonus;
        }

        drawStat("CARDS", cardPoints);
        drawStat("BUILDS", buildBonus);
        drawStat("SWEEPS", sweepBonus);
    }

    // ===== SETTINGS GEAR (DRAW LAST) =====
    if (settingsVisible) {
        glm::vec4 baseColor = glm::vec4(0.18f, 0.32f, 0.38f, 1.0f);
        glm::vec4 hoveredColor = glm::vec4(0.30f, 0.55f, 0.78f, 1.0f);
        glm::vec4 fillColor = m_SettingsButtonHovered ? hoveredColor : baseColor;
        glm::vec4 outlineColor = glm::vec4(0.03f, 0.05f, 0.06f, 1.0f);
        glm::vec2 outlineExtend{3.f, 3.f};
        glm::vec2 buttonPos{m_SettingsButtonRect.x, m_SettingsButtonRect.y};
        glm::vec2 buttonSize{m_SettingsButtonRect.w, m_SettingsButtonRect.h};

        Render2D::DrawQuad(buttonPos - outlineExtend, buttonSize + outlineExtend * 2.f, outlineColor);
        Render2D::DrawQuad(buttonPos, buttonSize, fillColor);

        glm::vec2 center = m_SettingsButtonRect.Center();
        float gearSpan = std::min(m_SettingsButtonRect.w, m_SettingsButtonRect.h) * 0.55f;
        float halfGear = gearSpan * 0.5f;
        float toothThickness = gearSpan * 0.22f;
        float toothLength = gearSpan * 0.45f;
        glm::vec4 gearColor = glm::vec4(0.94f, 0.96f, 0.98f, 1.0f);
        glm::vec4 hubColor = glm::mix(fillColor, glm::vec4(0.1f, 0.16f, 0.18f, 1.0f), 0.55f);

        Render2D::DrawQuad({center.x - halfGear, center.y - halfGear}, {gearSpan, gearSpan}, gearColor);

        glm::mat4 diagonal = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
        diagonal = glm::rotate(diagonal, glm::quarter_pi<float>(), {0.f,0.f,1.f});
        diagonal = glm::scale(diagonal, {gearSpan * 0.75f, gearSpan * 0.75f, 1.f});
        Render2D::DrawQuad(diagonal, gearColor);

        glm::vec2 horizontalTooth{toothLength, toothThickness};
        Render2D::DrawQuad({center.x - horizontalTooth.x * 0.5f, center.y - halfGear - toothThickness * 0.5f}, horizontalTooth, gearColor);
        Render2D::DrawQuad({center.x - horizontalTooth.x * 0.5f, center.y + halfGear - toothThickness * 0.5f}, horizontalTooth, gearColor);

        glm::vec2 verticalTooth{toothThickness, toothLength};
        Render2D::DrawQuad({center.x - halfGear - toothThickness * 0.5f, center.y - verticalTooth.y * 0.5f}, verticalTooth, gearColor);
        Render2D::DrawQuad({center.x + halfGear - toothThickness * 0.5f, center.y - verticalTooth.y * 0.5f}, verticalTooth, gearColor);

        float innerSpan = gearSpan * 0.46f;
        Render2D::DrawQuad({center.x - innerSpan * 0.5f, center.y - innerSpan * 0.5f}, {innerSpan, innerSpan}, hubColor);

        glm::mat4 innerDiagonal = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
        innerDiagonal = glm::rotate(innerDiagonal, glm::quarter_pi<float>(), {0.f,0.f,1.f});
        innerDiagonal = glm::scale(innerDiagonal, {innerSpan * 0.68f, innerSpan * 0.68f, 1.f});
        Render2D::DrawQuad(innerDiagonal, hubColor);

        float hubSize = gearSpan * 0.2f;
        glm::vec4 hubHighlight = glm::mix(gearColor, hubColor, 0.35f);
        Render2D::DrawQuad({center.x - hubSize * 0.5f, center.y - hubSize * 0.5f}, {hubSize, hubSize}, gearColor);
        float hubInset = hubSize * 0.55f;
        Render2D::DrawQuad({center.x - hubInset * 0.5f, center.y - hubInset * 0.5f}, {hubInset, hubInset}, hubHighlight);
    }
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
      bool showPendingOverlay = false;
      bool hideCardFromHand = false;
      if (m_PendingMove && m_PendingMove->player == p &&
          m_PendingMove->handIndex == static_cast<int>(i)) {
        showPendingOverlay = m_PendingMove->delay > 0.f;
        hideCardFromHand = m_PendingMove->delay <= 0.f;
      }

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

      if (hideCardFromHand) {
        continue;
      }

      if (isAI) {
        if (verticalSeat) {
          drawCardBack(drawRect, isCurrent, rotation);
        } else {
          drawCardBack(drawRect, isCurrent);
        }
        if (showPendingOverlay) {
          drawOverlay(glm::vec4(0.95f, 0.85f, 0.2f, 0.35f));
        }
      } else {
        bool selected =
            (isCurrent && m_Selection.handIndex &&
             *m_Selection.handIndex == static_cast<int>(i));
        if (verticalSeat) {
          drawCardFace(hand[i], drawRect, rotation, isCurrent, selected, false,
                       false);
        } else {
          drawCardFace(hand[i], drawRect, isCurrent, selected, false, false);
        }
        if (showPendingOverlay) {
          drawOverlay(glm::vec4(0.95f, 0.85f, 0.2f, 0.35f));
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

  if (m_PendingMove && m_PendingMove->player >= 0 &&
      m_PendingMove->handIndex >= 0 && m_PendingMove->delay <= 0.f) {
    if (m_PendingMove->player < static_cast<int>(m_PlayerHandRects.size()) &&
        m_PendingMove->handIndex <
            static_cast<int>(m_PlayerHandRects[m_PendingMove->player].size())) {
      const Rect &startRect =
          m_PlayerHandRects[m_PendingMove->player][m_PendingMove->handIndex];
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

      if (m_PendingMove->move.type == MoveType::Capture) {
        addLooseCenters(m_PendingMove->move.captureLooseIdx);
        addBuildCenters(m_PendingMove->move.captureBuildIdx);
      } else if (m_PendingMove->move.type == MoveType::Build) {
        addLooseCenters(m_PendingMove->move.buildUseLooseIdx);
      } else if (m_PendingMove->move.type == MoveType::ExtendBuild) {
        addLooseCenters(m_PendingMove->move.buildUseLooseIdx);
        addBuildCenters(m_PendingMove->move.captureBuildIdx);
      }

      if (targetCenters.empty()) {
        targetCenters.push_back(m_TableRect.Center());
      }

      glm::vec2 targetCenter(0.f);
      for (const auto &pt : targetCenters) targetCenter += pt;
      targetCenter /= static_cast<float>(targetCenters.size());

      float t = std::clamp(m_PendingMove->progress, 0.f, 1.f);
      glm::vec2 currentCenter = glm::mix(startCenter, targetCenter, t);
      Rect cardRect{currentCenter.x - m_CardWidth * 0.5f,
                    currentCenter.y - m_CardHeight * 0.5f, m_CardWidth,
                    m_CardHeight};
      drawCardFace(m_PendingMove->move.handCard, cardRect, false, false, false,
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

  ui::DrawText("ACTIONS", glm::vec2{m_ActionPanelRect.x + 10.f,
                                m_ActionPanelRect.y + 6.f},
           3.f, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));

  if (m_ActiveDifficulty != Difficulty::Easy) {
    std::string diffText = "DIFFICULTY: " + difficultyLabel(m_ActiveDifficulty);
    ui::DrawText(diffText, glm::vec2{m_ActionPanelRect.x + 10.f,
                                 m_ActionPanelRect.y + 28.f},
             2.6f, glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));
  }

  for (size_t i = 0; i < m_ActionEntries.size(); ++i) {
    const auto &entry = m_ActionEntries[i];
    glm::vec4 color =
        (m_HoveredAction == static_cast<int>(i))
            ? glm::vec4(0.9f, 0.6f, 0.3f, 1.0f)
            : glm::vec4(0.2f, 0.3f, 0.35f, 1.0f);
    Render2D::DrawQuad(glm::vec2{entry.rect.x, entry.rect.y},
                       glm::vec2{entry.rect.w, entry.rect.h}, color);
    ui::DrawText(entry.label,
             glm::vec2{entry.rect.x + 6.f, entry.rect.y + 10.f}, 3.f,
             glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
  }

  if (m_ActiveDifficulty != Difficulty::Easy) {
    bool hovered =
        m_ConfirmButtonRect.Contains(m_LastMousePos.x, m_LastMousePos.y);
    bool enabled = m_ConfirmableMove.has_value();
    ui::ButtonStyle confirmStyle;
    confirmStyle.baseColor = glm::vec4(0.25f, 0.55f, 0.38f, 1.0f);
    confirmStyle.hoveredColor = glm::vec4(0.35f, 0.65f, 0.48f, 1.0f);
    confirmStyle.disabledColor = glm::vec4(0.18f, 0.22f, 0.24f, 1.0f);
    confirmStyle.textStyle.scale = 3.f;
    confirmStyle.textStyle.color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
    confirmStyle.hoveredTextColor = confirmStyle.textStyle.color;
    confirmStyle.disabledTextColor = glm::vec4(0.45f, 0.45f, 0.45f, 1.0f);
    confirmStyle.drawOutline = false;
    ui::DrawButton(m_ConfirmButtonRect, "CONFIRM MOVE", confirmStyle,
                   ui::ButtonState{hovered, enabled});
  }

  if (m_Selection.handIndex) {
    bool hovered =
        m_CancelButtonRect.Contains(m_LastMousePos.x, m_LastMousePos.y);
    ui::ButtonStyle cancelStyle;
    cancelStyle.baseColor = glm::vec4(0.35f, 0.18f, 0.18f, 1.0f);
    cancelStyle.hoveredColor = glm::vec4(0.45f, 0.22f, 0.22f, 1.0f);
    cancelStyle.disabledColor = cancelStyle.baseColor;
    cancelStyle.textStyle.scale = 3.2f;
    cancelStyle.textStyle.color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    cancelStyle.hoveredTextColor = cancelStyle.textStyle.color;
    cancelStyle.disabledTextColor = cancelStyle.textStyle.color;
    cancelStyle.drawOutline = false;
    ui::DrawButton(m_CancelButtonRect, "CANCEL", cancelStyle,
                   ui::ButtonState{hovered, true});
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

  ui::DrawText(m_PromptHeader,
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
        ui::DrawText(text, glm::vec2{m_PromptBoxRect.x + 16.f, lineY}, 3.2f,
                 glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
        lineY += 24.f;
      }
    }
  } else if (m_PromptMode == PromptMode::PlayerSetup) {
    ui::DrawText("SELECT TOTAL PLAYERS", glm::vec2{m_PromptBoxRect.x + 16.f,
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
      ui::DrawText(label, glm::vec2{textX, rect.y + 10.f}, 3.2f,
               glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    }

    ui::DrawText("ASSIGN SEAT ROLES", glm::vec2{m_PromptBoxRect.x + 16.f,
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
      ui::DrawText(label, glm::vec2{rect.x + 12.f, rect.y + 8.f}, 3.f,
               glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    }

    int aiCount = std::max(0, m_MenuSelectedPlayers - m_MenuSelectedHumans);
    ui::DrawText("HUMANS " + std::to_string(m_MenuSelectedHumans) + " | AI " +
                 std::to_string(aiCount),
             glm::vec2{m_PromptBoxRect.x + 16.f, m_MenuSummaryTextY},
             3.f, glm::vec4(0.8f, 0.85f, 0.9f, 1.0f));
    ui::DrawText("CLICK TO TOGGLE HUMAN / AI",
             glm::vec2{m_PromptBoxRect.x + 16.f, m_MenuInstructionTextY},
             2.8f, glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));
  } else if (m_PromptMode == PromptMode::MainMenuSettings) {
    ui::DrawText("SELECT DIFFICULTY", glm::vec2{m_PromptBoxRect.x + 16.f,
                                           m_PromptBoxRect.y + 64.f},
             3.f, glm::vec4(0.85f, 0.9f, 0.95f, 1.0f));
    std::array<Difficulty, 3> difficulties = {Difficulty::Easy,
                                              Difficulty::Medium,
                                              Difficulty::Hard};
    for (size_t i = 0; i < difficulties.size() &&
                       i < m_DifficultyOptionRects.size();
         ++i) {
      const Rect &rect = m_DifficultyOptionRects[i];
      bool selected = (m_MenuDifficulty == difficulties[i]);
      bool hovered = rect.Contains(m_LastMousePos.x, m_LastMousePos.y);
      glm::vec4 color = selected ? glm::vec4(0.28f, 0.58f, 0.38f, 1.0f)
                                 : glm::vec4(0.2f, 0.3f, 0.35f, 1.0f);
      if (hovered) {
        color = glm::mix(color, glm::vec4(0.9f, 0.7f, 0.35f, 1.0f), 0.35f);
      }
      Render2D::DrawQuad(glm::vec2{rect.x, rect.y}, glm::vec2{rect.w, rect.h},
                         color);
      std::string label = difficultyLabel(difficulties[i]);
      glm::vec2 metrics = ui::MeasureText(label, 3.2f);
      float textX = rect.x + rect.w * 0.5f - metrics.x * 0.5f;
      float textY = rect.y + rect.h * 0.5f - metrics.y * 0.5f;
      ui::DrawText(label, glm::vec2{textX, textY}, 3.2f,
               glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
    }
    std::string desc = difficultyDescription(m_MenuDifficulty);
    ui::TextStyle descStyle;
    descStyle.scale = 3.f;
    descStyle.color = glm::vec4(0.8f, 0.85f, 0.9f, 1.0f);
    float descX = m_PromptBoxRect.x + 16.f;
    float descY = m_PromptBoxRect.y + kMainMenuSettingsDescriptionTop;
    float descMaxWidth = m_PromptBoxRect.w - 32.f;
    auto descLines = wrapText(desc, descStyle, descMaxWidth);
    drawWrappedLines(descLines, glm::vec2{descX, descY}, descStyle);
  } else if (m_PromptMode == PromptMode::HowToPlay) {
    ui::TextStyle howStyle;
    howStyle.scale = 2.6f;
    howStyle.color = glm::vec4(0.85f, 0.9f, 0.95f, 1.0f);
    float textX = m_PromptBoxRect.x + 16.f;
    float textY = m_PromptBoxRect.y + kPromptTextStart;
    float maxWidth = m_PromptBoxRect.w - 32.f;
    auto howLines = wrapText(joinLines(kHowToPlayLines), howStyle, maxWidth);
    drawWrappedLines(howLines, glm::vec2{textX, textY}, howStyle);
  } else if (m_PromptMode == PromptMode::Settings) {
    float textX = m_PromptBoxRect.x + 16.f;
    float textY = m_PromptBoxRect.y + kPromptTextStart;
    float maxWidth = m_PromptBoxRect.w - 32.f;
    const float paragraphSpacing = 14.f;

    ui::TextStyle primaryStyle;
    primaryStyle.scale = 3.2f;
    primaryStyle.color = glm::vec4(0.85f, 0.9f, 0.95f, 1.0f);
    ui::TextStyle secondaryStyle;
    secondaryStyle.scale = 3.f;
    secondaryStyle.color = glm::vec4(0.8f, 0.85f, 0.9f, 1.0f);

    auto paragraph1 = wrapText(kSettingsParagraph1, primaryStyle, maxWidth);
    float height1 = drawWrappedLines(paragraph1, glm::vec2{textX, textY},
                                     primaryStyle);
    textY += height1 + paragraphSpacing;

    auto paragraph2 = wrapText(kSettingsParagraph2, secondaryStyle, maxWidth);
    float height2 = drawWrappedLines(paragraph2, glm::vec2{textX, textY},
                                     secondaryStyle);
    textY += height2 + paragraphSpacing;

    auto paragraph3 = wrapText(kSettingsParagraph3, secondaryStyle, maxWidth);
    drawWrappedLines(paragraph3, glm::vec2{textX, textY}, secondaryStyle);
  }

  bool primaryEnabled = true;
  if (m_PromptMode == PromptMode::PlayerSetup) {
    primaryEnabled = m_MenuSelectedHumans > 0 && m_MenuSelectedPlayers > 0;
  }

  ui::ButtonStyle primaryStyle;
  primaryStyle.baseColor = glm::vec4(0.25f, 0.55f, 0.85f, 1.0f);
  primaryStyle.hoveredColor = glm::vec4(0.35f, 0.65f, 0.95f, 1.0f);
  primaryStyle.disabledColor = glm::vec4(0.2f, 0.2f, 0.22f, 1.0f);
  primaryStyle.textStyle.scale = 3.2f;
  primaryStyle.textStyle.color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
  primaryStyle.hoveredTextColor = primaryStyle.textStyle.color;
  primaryStyle.disabledTextColor = glm::vec4(0.45f, 0.45f, 0.45f, 1.0f);
  primaryStyle.drawOutline = false;
  ui::ButtonState primaryState{
      m_PromptButtonRect.Contains(m_LastMousePos.x, m_LastMousePos.y),
      primaryEnabled};
  ui::DrawButton(m_PromptButtonRect, m_PromptButtonLabel, primaryStyle,
                 primaryState);

  if (!m_PromptSecondaryButtonLabel.empty()) {
    ui::ButtonStyle secondaryStyle;
    secondaryStyle.baseColor = glm::vec4(0.45f, 0.22f, 0.22f, 1.0f);
    secondaryStyle.hoveredColor = glm::vec4(0.65f, 0.32f, 0.32f, 1.0f);
    secondaryStyle.disabledColor = secondaryStyle.baseColor;
    secondaryStyle.textStyle.scale = 3.2f;
    secondaryStyle.textStyle.color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    secondaryStyle.hoveredTextColor = secondaryStyle.textStyle.color;
    secondaryStyle.disabledTextColor = secondaryStyle.textStyle.color;
    secondaryStyle.drawOutline = false;
    ui::ButtonState secondaryState{
        m_PromptSecondaryButtonRect.Contains(m_LastMousePos.x,
                                             m_LastMousePos.y),
        true};
    ui::DrawButton(m_PromptSecondaryButtonRect, m_PromptSecondaryButtonLabel,
                   secondaryStyle, secondaryState);
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

  glm::vec2 titleMetrics = ui::MeasureText(kMainMenuTitleText, kMainMenuTitleScale);
  glm::vec2 subtitleMetrics =
      ui::MeasureText(kMainMenuSubtitleText, kMainMenuSubtitleScale);
  glm::vec2 footerMetrics =
      ui::MeasureText(kMainMenuFooterText, kMainMenuFooterScale);

  float titleY = height * 0.25f - titleMetrics.y;
  float titleToSubtitleSpacing = titleMetrics.y * kTitleSubtitleSpacingFactor;
  float subtitleY = titleY + titleMetrics.y + titleToSubtitleSpacing;
  float buttonsToFooterSpacing =
      footerMetrics.y * kButtonsFooterSpacingFactor;

  Render2D::DrawQuad(glm::vec2{0.f, 0.f}, glm::vec2{width, height},
                     glm::vec4(0.06f, 0.12f, 0.15f, 1.0f));

  ui::DrawText(kMainMenuTitleText,
           glm::vec2{centerX - titleMetrics.x * 0.5f, titleY},
           kMainMenuTitleScale,
           glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
  ui::DrawText(kMainMenuSubtitleText,
           glm::vec2{centerX - subtitleMetrics.x * 0.5f, subtitleY},
           kMainMenuSubtitleScale,
           glm::vec4(0.8f, 0.85f, 0.9f, 1.0f));

  ui::ButtonStyle menuButtonStyle;
  menuButtonStyle.baseColor = glm::vec4(0.18f, 0.32f, 0.38f, 1.0f);
  menuButtonStyle.hoveredColor = glm::vec4(0.30f, 0.55f, 0.78f, 1.0f);
  menuButtonStyle.disabledColor = menuButtonStyle.baseColor;
  menuButtonStyle.textStyle.scale = 4.f;
  menuButtonStyle.textStyle.color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
  menuButtonStyle.hoveredTextColor = menuButtonStyle.textStyle.color;
  menuButtonStyle.disabledTextColor = menuButtonStyle.textStyle.color;
  menuButtonStyle.drawOutline = true;
  menuButtonStyle.outlineExtend = glm::vec2{4.f, 4.f};
  menuButtonStyle.outlineColor = glm::vec4(0.03f, 0.05f, 0.06f, 1.0f);

  ui::DrawButton(m_MainMenuStartButtonRect, "START", menuButtonStyle,
                 ui::ButtonState{m_MainMenuStartHovered, true});
  ui::DrawButton(m_MainMenuSettingsButtonRect, "SETTINGS", menuButtonStyle,
                 ui::ButtonState{m_MainMenuSettingsHovered, true});
  ui::DrawButton(m_MainMenuHowToButtonRect, "HOW TO PLAY", menuButtonStyle,
                 ui::ButtonState{m_MainMenuHowToHovered, true});

  float footerY = m_MainMenuHowToButtonRect.y + m_MainMenuHowToButtonRect.h +
                  buttonsToFooterSpacing;
  float maxFooterY = height - kMainMenuBottomMargin - footerMetrics.y;
  footerY = std::min(footerY, maxFooterY);
  ui::DrawText(kMainMenuFooterText,
           glm::vec2{centerX - footerMetrics.x * 0.5f, footerY},
           kMainMenuFooterScale,
           glm::vec4(0.75f, 0.8f, 0.85f, 1.0f));
}

void KasinoGame::OnRender() { drawScene(); }
