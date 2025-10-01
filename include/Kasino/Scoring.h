#pragma once
#include "GameLogic.h"

// shared helpers (inline so multiple includes are fine)
inline bool IsAce(const Card& c)         { return c.rank == Rank::Ace; }
inline bool IsBigCasino(const Card& c)   { return c.rank == Rank::Two && c.suit == Suit::Spades; }     // 2♠
inline bool IsLittleCasino(const Card& c){ return c.rank == Rank::Ten && c.suit == Suit::Diamonds; }  // 10♦

  inline std::vector<ScoreLine> ScoreRound(GameState& gs) {
    std::vector<ScoreLine> score(gs.numPlayers);

    // Count piles
    for (int p=0; p<gs.numPlayers; ++p) {
      auto& sc = score[p];
      for (auto& c : gs.players[p].pile) {
	sc.cardsCount++;
	if (c.suit==Suit::Spades) sc.spadesCount++;
	if (IsAce(c)) sc.aces++;
	if (IsBigCasino(c)) sc.bigCasino = 1;
	if (IsLittleCasino(c)) sc.littleCasino = 2; // worth 2 pts
      }
      sc.captureBonuses = gs.players[p].captureBonuses;
      sc.buildCaptureBonuses = gs.players[p].buildCaptureBonuses;
      sc.sweepBonuses = gs.players[p].sweepBonuses;
    }

    // Most cards (3)
    int maxCards = 0;
    for (auto& sc : score) maxCards = std::max(maxCards, sc.cardsCount);
    // If tie, no points (traditional Cassino)
    int mcWinners=0, mcIdx=-1;
    for (int i=0;i<(int)score.size();++i) if (score[i].cardsCount==maxCards){ mcWinners++; mcIdx=i; }
    if (mcWinners==1) score[mcIdx].mostCards = 3;

    // Most spades (1)
    int maxSp = 0;
    for (auto& sc : score) maxSp = std::max(maxSp, sc.spadesCount);
    int msWinners=0, msIdx=-1;
    for (int i=0;i<(int)score.size();++i) if (score[i].spadesCount==maxSp){ msWinners++; msIdx=i; }
    if (msWinners==1) score[msIdx].mostSpades = 1;

    // Totals
    for (auto& sc : score) {
      sc.total = sc.mostCards + sc.mostSpades + sc.bigCasino + sc.littleCasino +
                 sc.aces + sc.captureBonuses + sc.buildCaptureBonuses +
                 sc.sweepBonuses;
    }
    return score;
  }

