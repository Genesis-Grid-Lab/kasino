#pragma once
#include "Card.h"
#include <vector>
#include <cstdint>
#include <string>

namespace Casino {

// A build on table (value + owner). Owner may be -1 (unowned) but normally the player who created/last extended it.
struct Build {
  int value = 0;                  // 2..13 typically; A=1
  int ownerPlayer = -1;           // index of player expected to capture later
  std::vector<Card> cards;        // constituent loose cards used to form the build (for display/debug)
};

enum class MoveType : uint8_t { Capture, Build, ExtendBuild, Trail };

struct Move {
  MoveType type{};
  Card     handCard{};                // the card being played from hand

  // For Capture: which loose table cards and which build indices to take
  std::vector<int> captureLooseIdx;   // indices into GameState::tableLoose
  std::vector<int> captureBuildIdx;   // indices into GameState::builds

  // For Build/ExtendBuild
  int buildTargetValue = 0;           // declared build value (must equal RankValue(hand) + sum(loose))
  std::vector<int> buildUseLooseIdx;  // loose cards you combine with hand card to make/extend

  std::string Debug() const;
};

inline std::string Move::Debug() const {
  auto mt = [this]{
    switch(type){
    case MoveType::Capture: return "Capture";
    case MoveType::Build: return "Build";
    case MoveType::ExtendBuild: return "ExtendBuild";
    default: return "Trail";
    }
  }();
  return mt;
}

} // namespace Casino
