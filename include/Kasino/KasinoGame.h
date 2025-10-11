#pragma once

#include "core/Types.h"
#include "app/Game.h"
#include "Kasino/GameLogic.h"
#include "Kasino/Scoring.h"
#include "input/InputSystem.h"
#include "gfx/ITexture2D.h"
#include "audio/IAudioBuffer.h"
#include "audio/IAudioSource.h"
#include "audio/SoundSystem.h"

#include <glm/glm.hpp>

class KasinoGame : public Game {
 public:
 bool OnStart() override;
 void OnUpdate(float dt) override;
 void OnRender() override;
 void OnStop() override;
 ~KasinoGame();

 private:
  enum class Phase { MainMenu, Playing, RoundSummary, MatchSummary };

  enum class PromptMode {
    None,
    RoundSummary,
    MatchSummary,
    HandSummary,
    PlayerSetup,
    Settings,
    MainMenuSettings,
    HowToPlay
  };

  enum class Difficulty { Easy, Medium, Hard };

  enum class PromptAction { Primary, Secondary };
private:
  // tmp
  glm::mat4 buildCardTransform(const Rect &rect, float rotation);

  void startNewMatch();
  void startNextRound();
  void updateRoundScorePreview();
  void beginDealAnimation();  
  void updateLegalMoves();
  void updateMainMenuLayout();
  void updateLayout();
  void updatePromptLayout();
  void updateMenuHumanCounts();
  void updateActionOptions();
  void layoutActionEntries();
  void updateHoveredAction(float mx, float my);
  void refreshHighlights();
  void processMainMenuInput(float mx, float my);
  void processInput(float mx, float my);
  bool playAiTurn();
  bool handlePromptInput(float mx, float my);
  void selectHandCard(int player, int index);
  void toggleLooseCard(int idx);
  void toggleBuild(int idx);
  void applyMove(const Move &mv);
  void beginPendingMove(const Move &mv, int player, int handIndex,
                        float delay);
  void handleRoundEnd();
  void handlePrompt(PromptAction action);

  void drawScene();
  void drawMainMenu();
  void drawScoreboard();
  void drawHands();
  void drawTable();
  void drawActionPanel();
  void drawPromptOverlay();

  void drawCardFace(const Card &card, const Rect &r, bool isCurrent,
                    bool selected, bool legal, bool hovered);
  void drawCardFace(const Card &card, const Rect &r, float rotation,
                    bool isCurrent, bool selected, bool legal, bool hovered);
  void drawCardBack(const Rect &r, bool isCurrent);
  void drawCardBack(const Rect &r, bool isCurrent, float rotation);
  void drawBuildFace(const Build &build, const Rect &r, bool legal,
                     bool hovered, bool selected);

  std::string moveLabel(const Move &mv) const;
  std::string moveLabelForDifficulty(const Move &mv, Difficulty difficulty) const;
  std::string difficultyLabel(Difficulty difficulty) const;
  std::string difficultyDescription(Difficulty difficulty) const;
  bool selectionMatches(const Move &mv) const;
  bool movesEquivalent(const Move &a, const Move &b) const;
  bool selectionCompatible(const Move &mv) const;
  void loadCardTextures();
  std::string cardTextureKey(const Card &card) const;
  std::string cardTexturePath(const Card &card) const;
  std::string cardRankString(Rank rank) const;
  std::string cardSuitFolder(Suit suit) const;

  void playCardSlideSound();
  void PlayEventSound(const Ref<IAudioBuffer>& buffer);
private:
  std::unique_ptr<InputSystem> m_Input;
  Ref<IAudioSource> m_GlobAudioSource;
  Ref<IAudioBuffer> m_Audio_1;
  Ref<IAudioBuffer> m_card_slide_1;
  Ref<IAudioBuffer> m_card_slide_2;
  Ref<IAudioBuffer> m_sndBuild;
  Ref<IAudioBuffer> m_sndTrail;
  Ref<IAudioBuffer> m_sndTake;
  Ref<IAudioBuffer> m_sndSweep;
  Ref<IAudioBuffer> m_sndWin;
  Ref<IAudioBuffer> m_sndRoundEnd;
  Ref<IAudioBuffer> m_sndNewGame;
  int m_NextCardSlideIndex = 0;
  GameState m_State;
  std::vector<Move> m_LegalMoves;
  std::vector<ActionEntry> m_ActionEntries;
  Selection m_Selection;
  Phase m_Phase = Phase::MainMenu;
  PromptMode m_PromptMode = PromptMode::None;

  Rect m_MainMenuStartButtonRect{};
  Rect m_MainMenuSettingsButtonRect{};
  Rect m_MainMenuHowToButtonRect{};
  bool m_MainMenuStartHovered = false;
  bool m_MainMenuSettingsHovered = false;
  bool m_MainMenuHowToHovered = false;

  int m_MenuSelectedPlayers = 2;
  int m_MenuSelectedHumans = 1;
  std::array<bool, 4> m_MenuSeatIsAI{false, true, true, true};
  Difficulty m_MenuDifficulty = Difficulty::Easy;
  Difficulty m_ActiveDifficulty = Difficulty::Easy;
  std::vector<Rect> m_MenuPlayerCountRects;
  std::vector<Rect> m_MenuSeatToggleRects;
  std::vector<Rect> m_DifficultyOptionRects;
  float m_MenuSummaryTextY = 0.f;
  float m_MenuInstructionTextY = 0.f;
  int m_HumanSeatCount = 1;
  std::vector<bool> m_SeatIsAI;
  std::vector<bool> m_IsAiPlayer;
  glm::vec2 m_LastMousePos{0.f, 0.f};

  std::vector<int> m_TotalScores;
  std::vector<RunningScore> m_CurrentRoundScores;
  std::vector<ScoreLine> m_LastRoundScores;
  int m_TargetScore = 21;
  int m_RoundNumber = 1;
  int m_WinningPlayer = -1;

  std::mt19937 m_Rng{std::random_device{}()};

  // Layout
  float m_CardWidth = 56.f;
  float m_CardHeight = 80.f;
  Rect m_TableRect{};
  Rect m_ActionPanelRect{};
  Rect m_PromptBoxRect{};
  Rect m_PromptButtonRect{};
  Rect m_PromptSecondaryButtonRect{};
  Rect m_CancelButtonRect{};
  Rect m_ConfirmButtonRect{};
  Rect m_SettingsButtonRect{};
  float m_ScoreboardHeight = 132.f;
  bool m_SettingsButtonHovered = false;

  std::vector<std::vector<Rect>> m_PlayerHandRects;
  std::vector<SeatLayout> m_PlayerSeatLayouts;
  std::vector<Rect> m_LooseRects;
  std::vector<Rect> m_BuildRects;

  std::vector<bool> m_LooseHighlights;
  std::vector<bool> m_BuildHighlights;

  struct PendingMove {
    Move move{};
    int player = -1;
    int handIndex = -1;
    float delay = 0.f;
    float progress = 0.f;
  };

  std::optional<PendingMove> m_PendingMove;
  std::vector<bool> m_PendingLooseHighlights;
  std::vector<bool> m_PendingBuildHighlights;
  std::optional<Move> m_ConfirmableMove;
  bool m_ConfirmAmbiguous = false;

  int m_HoveredAction = -1;
  std::set<int> m_HoveredLoose;
  std::set<int> m_HoveredBuilds;

  bool m_ShowPrompt = false;
  std::string m_PromptHeader;
  std::string m_PromptButtonLabel;
  std::string m_PromptSecondaryButtonLabel;

  std::vector<DealAnim> m_DealQueue;
  std::vector<int> m_DealtCounts;
  bool m_IsDealing = false;
  glm::vec2 m_DeckOrigin{0.f, 0.f};  

  const std::array<glm::vec4, 4> m_PlayerColors = {
      glm::vec4(0.85f, 0.35f, 0.30f, 1.0f),
      glm::vec4(0.25f, 0.55f, 0.95f, 1.0f),
      glm::vec4(0.35f, 0.80f, 0.45f, 1.0f),
      glm::vec4(0.90f, 0.70f, 0.25f, 1.0f)};

  std::unordered_map<std::string, Ref<ITexture2D>> m_CardTextures;
  Ref<ITexture2D> m_CardBackTexture;
};

