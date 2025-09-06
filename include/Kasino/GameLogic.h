#pragma once
#include "GameState.h"
#include "Deck.h"
#include <vector>
#include <utility>

namespace Casino {

  // Dealing & flow
  void StartRound(GameState& gs, int numPlayers=2, uint32_t shuffleSeed=0);
  bool DealNextHands(GameState& gs); // returns false when no stock
  void AdvanceTurn(GameState& gs);

  // Move generation & execution
  std::vector<Move> LegalMoves(const GameState& gs);
  bool ApplyMove(GameState& gs, const Move& mv); // returns true if applied

  // Scoring at end of round
  struct ScoreLine { int total=0; int mostCards=0; int mostSpades=0; int bigCasino=0; int littleCasino=0; int aces=0; int sweeps=0; int cardsCount=0; int spadesCount=0; };
  std::vector<ScoreLine> ScoreRound(GameState& gs);

  // Utility
  int CardSumValue(const std::vector<Card>& v);
}
