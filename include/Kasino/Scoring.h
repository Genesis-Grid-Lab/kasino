#pragma once
#include "GameLogic.h"

// shared helpers (inline so multiple includes are fine)
  inline std::vector<ScoreLine> ScoreRound(GameState& gs) {
    std::vector<ScoreLine> score(gs.numPlayers);

    for (int p = 0; p < gs.numPlayers; ++p) {
      auto &sc = score[p];
      const auto &player = gs.players[p];
      sc.capturedCardPoints = player.capturedCardPoints;
      sc.buildBonus = player.buildBonus;
      sc.sweepBonus = player.sweepBonus;
      sc.total = sc.capturedCardPoints + sc.buildBonus + sc.sweepBonus;
    }

    return score;
  }

