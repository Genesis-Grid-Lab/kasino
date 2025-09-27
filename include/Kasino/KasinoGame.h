#pragma once

#include "app/Game.h"
#include "Kasino/GameLogic.h"
#include "Kasino/Scoring.h"

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class InputSystem;

class KasinoGame : public Game {
 public:
 bool OnStart() override;
 void OnUpdate(float dt) override;
 void OnRender() override;
 void OnStop() override;
 ~KasinoGame();

 private:
  enum class Phase { Playing, RoundSummary, MatchSummary };

  struct Rect {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;

    bool Contains(float px, float py) const;
    glm::vec2 Center() const;
  };

  struct Selection {
    std::optional<int> handIndex;
    std::set<int> loose;
    std::set<int> builds;
    void Clear();
  };

  struct ActionEntry {
    Casino::Move move;
    std::string label;
    Rect rect;
  };

  void startNewMatch();
  void startNextRound();
  void updateLegalMoves();
  void updateLayout();
  void layoutActionEntries();
  void updateHoveredAction(float mx, float my);
  void refreshHighlights();
  void processInput(float mx, float my);
  void selectHandCard(int player, int index);
  void toggleLooseCard(int idx);
  void toggleBuild(int idx);
  void applyMove(const Casino::Move &mv);
  void handleRoundEnd();
  void handlePrompt();

  void drawScene();
  void drawScoreboard();
  void drawHands();
  void drawTable();
  void drawActionPanel();
  void drawPromptOverlay();

  void drawCardFace(const Casino::Card &card, const Rect &r, bool isCurrent,
                    bool selected, bool legal, bool hovered);
  void drawBuildFace(const Casino::Build &build, const Rect &r, bool legal,
                     bool hovered, bool selected);
  void drawText(const std::string &text, glm::vec2 pos, float scale,
                const glm::vec4 &color);

  std::string moveLabel(const Casino::Move &mv) const;
  bool selectionCompatible(const Casino::Move &mv) const;

  std::unique_ptr<InputSystem> m_Input;
  Casino::GameState m_State;
  std::vector<Casino::Move> m_LegalMoves;
  std::vector<ActionEntry> m_ActionEntries;
  Selection m_Selection;
  Phase m_Phase = Phase::Playing;

  std::vector<int> m_TotalScores;
  std::vector<Casino::ScoreLine> m_LastRoundScores;
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
  Rect m_CancelButtonRect{};
  float m_ScoreboardHeight = 100.f;

  std::vector<std::vector<Rect>> m_PlayerHandRects;
  std::vector<Rect> m_LooseRects;
  std::vector<Rect> m_BuildRects;

  std::vector<bool> m_LooseHighlights;
  std::vector<bool> m_BuildHighlights;

  int m_HoveredAction = -1;
  std::set<int> m_HoveredLoose;
  std::set<int> m_HoveredBuilds;

  bool m_ShowPrompt = false;
  std::string m_PromptHeader;
  std::string m_PromptButtonLabel;

  const std::array<glm::vec4, 4> m_PlayerColors = {
      glm::vec4(0.85f, 0.35f, 0.30f, 1.0f),
      glm::vec4(0.25f, 0.55f, 0.95f, 1.0f),
      glm::vec4(0.35f, 0.80f, 0.45f, 1.0f),
      glm::vec4(0.90f, 0.70f, 0.25f, 1.0f)};
};

