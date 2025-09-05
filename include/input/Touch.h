#pragma once

#include <cstdint>

enum class TouchPhase : uint8_t { Began, Moved, Stationary, Ended, Cancelled };

struct TouchPoint {
  std::int64_t id; // platform finger id
  float x, y;      // logical coordinates
  float dx, dy;    // optinal deltas
  float pressure;  // 0..1 if available
  TouchPhase phase;
};

