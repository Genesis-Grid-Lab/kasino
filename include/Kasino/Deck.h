#pragma once
#include "Card.h"
#include <vector>
#include <random>
#include <algorithm>

struct Deck {
  std::vector<Card> cards;

  void Reset() {
    cards.clear();
    for (int s=0; s<4; ++s)
      for (int r=1; r<=13; ++r)
	cards.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
  }

  void Shuffle(uint32_t seed=0) {
    std::mt19937 rng(seed ? seed : std::random_device{}());
    std::shuffle(cards.begin(), cards.end(), rng);
  }

  bool Empty() const { return cards.empty(); }

  // Deal n cards to target (assumes enough cards)
  template<class Out>
  void Deal(int n, Out out) {
    for (int i=0;i<n && !cards.empty(); ++i) {
      *out++ = cards.back();
      cards.pop_back();
    }
  }
};
