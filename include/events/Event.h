#pragma once

#include "input/Key.h"
#include "input/Mouse.h"
#include "input/Touch.h"

enum class EventType : uint16_t {
  WindowClose,
  WindowResize,
  ContentScale, // DPR change

  AppPause,
  AppResume,
  AppFocus,
  AppUnfocus,

  KeyDown,
  KeyUp,
  KeyChar, // KeyChar is text input

  MouseDown,
  MouseUp,
  MouseMove,
  MouseWheel,

  Touch, // generic touch (batch)
};

struct EWindowResize {
  int fbWidth, fbHeight;
  float dpr;
};

struct EContentScale {
  float dpr;
};

struct EKey {
  Key key;
  bool repeat;
};

struct EKeyChar {
  unsigned int codepoint;
};

struct EMouseButton {
  MouseButton button;
  float x, y;
};

struct EMouseMove {
  float x, y;
};

struct EMouseWheel {
  float dx, dy;
};

struct ETouch{
  TouchPoint points[10];
  int count;  
}; // simple fixed batch
