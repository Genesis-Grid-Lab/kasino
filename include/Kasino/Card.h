#pragma once
#include <cstdint>
#include <string>

namespace Casino {

enum class Suit : uint8_t { Clubs, Diamonds, Hearts, Spades };
enum class Rank : uint8_t { Ace=1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack=11, Queen=12, King=13 };

inline int RankValue(Rank r) { return static_cast<int>(r); } // A=1, J=11, Q=12, K=13

struct Card {
  Rank rank{};
  Suit suit{};

  Card() = default;
  Card(Rank r, Suit s) : rank(r), suit(s) {}

  bool operator==(const Card& o) const { return rank==o.rank && suit==o.suit; }
  bool operator!=(const Card& o) const { return !(*this==o); }

  std::string ToString() const {
    static const char* R[]={"?","A","2","3","4","5","6","7","8","9","10","J","Q","K"};
    static const char* S[]={"C","D","H","S"};
    return std::string(R[RankValue(rank)]) + S[static_cast<int>(suit)];
  }
};

} // namespace Casino
