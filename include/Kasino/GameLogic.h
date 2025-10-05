#pragma once
#include "GameState.h"
#include "Deck.h"
#include <vector>
#include <utility>
#include <set>
#include "gfx/Render2D.h"

  // Dealing & flow
  void StartRound(GameState& gs, int numPlayers=2, uint32_t shuffleSeed=0);
  bool DealNextHands(GameState& gs); // returns false when no stock
  void AdvanceTurn(GameState& gs);

  // Move generation & execution
  std::vector<Move> LegalMoves(const GameState& gs);
  bool ApplyMove(GameState& gs, const Move& mv); // returns true if applied

  // Scoring at end of round
  struct ScoreLine {
    int total = 0;
    int capturedCardPoints = 0;
    int buildBonus = 0;
    int sweepBonus = 0;
  };
  std::vector<ScoreLine> ScoreRound(GameState& gs);

  // Utility
  int CardSumValue(const std::vector<Card>& v);

struct Selection {
    std::optional<int> handIndex;
    std::set<int> loose;
    std::set<int> builds;
    void Clear();
  };

  struct ActionEntry {
    Move move;
    std::string label;
    Rect rect;
  };

  enum class SeatOrientation { Horizontal, Vertical };

  struct SeatLayout {
    Rect anchor;
    SeatOrientation orientation = SeatOrientation::Horizontal;
    float visibleFraction = 1.0f;
  };

  struct RunningScore {
    ScoreLine line;
  };

  struct DealAnim {
    int player = -1;
    int handIndex = -1;
    Card card{};
    float delay = 0.f;
    float progress = 0.f;
  };  
