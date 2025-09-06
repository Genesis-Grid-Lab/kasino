#pragma once
#include "Card.h"
#include "Move.h"
#include <vector>
#include <optional>

struct PlayerState {
  std::vector<Card> hand;
  std::vector<Card> pile;   // captured
  int sweeps = 0;           // sweep count this round
};

struct TableState {
  std::vector<Card> loose;     // loose face-up cards
  std::vector<Build> builds;   // active builds
};

struct DealState {
  int dealer = 0;     // player index who deals this round
  int nextToDeal = 0; // counts deals; not strictly necessary
};

struct GameState {
  int numPlayers = 2;
  int current = 0;        // whose turn (0..numPlayers-1)

  std::vector<PlayerState> players;
  TableState table;
  std::vector<Card> stock;     // remaining undealt deck

  int lastCaptureBy = -1; // who last captured (for end-of-round sweep of table)

  // helper
  const PlayerState& CurPlayer() const { return players[current]; }
  PlayerState&       CurPlayer()       { return players[current]; }

  bool RoundOver() const {
    // round ends when stock empty and all hands empty
    if (!stock.empty()) return false;
    for (auto& p : players) if (!p.hand.empty()) return false;
    return true;
  }
};
