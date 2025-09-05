#pragma once
#include <bitset>
#include "core/Types.h"
#include "input/Key.h"
#include "input/Mouse.h"
#include "input/Touch.h"
#include "events/EventBus.h"

enum class ButtonState : uint8_t { Up = 0, Pressed, Held, Released };

class InputSystem {
 public:
  explicit InputSystem(EventBus &bus);

  // Call one per frame (before PollEvents)
  void BeginFrame();

  // Keyboard
  bool IsKeyDown(Key k) const {
    return stateOf(k) == ButtonState::Held ||
      stateOf(k) == ButtonState::Pressed;
  }  
  bool IsKeyUp(Key k) const {
    return stateOf(k) == ButtonState::Up || stateOf(k) == ButtonState::Released;
  }  
  bool WasKeyPressed(Key k) const {
    return stateOf(k) == ButtonState::Pressed;
  }    
  bool WasKeyReleased(Key k) const {
    return stateOf(k) == ButtonState::Released;
  }

  // Mouse
  bool IsMouseDown(MouseButton b) const { return m_MouseDown.test((size_t)b); }  
  bool WasMousePressed(MouseButton b) const {
    return m_MousePressedThisFrame.count((size_t)b) != 0;
  }  
  bool WasMouseReleased(MouseButton b) const {
    return m_MouseReleasedThisFrame.count((size_t)b) != 0;
  }

  float MouseX() const { return m_MouseX; }
    float MouseY() const { return m_MouseY; }
    float MouseDX() const { return m_MouseDX; }   // since last BeginFrame()
    float MouseDY() const { return m_MouseDY; }
    float WheelDX() const { return m_WheelDX; }   // cleared in BeginFrame()
    float WheelDY() const { return m_WheelDY; }

    // Touch
    const std::vector<TouchPoint> &Touches() const { return m_Touches; }

  private:
    // Event handlers
    void onKey(const EKey& e, bool down);
    void onMouseButton(const EMouseButton& e, bool down);
    void onMouseMove(const EMouseMove& e);
    void onMouseWheel(const EMouseWheel& e);
    void onTouch(const ETouch& e);

    ButtonState& stateRef(Key k);
    ButtonState stateOf(Key k) const;

  private:
    EventBus &m_Bus;

    // Keyboard: compact table by uint16_t key code range (0..512 ok)
    static constexpr size_t MAX_KEY = 512;
    std::array<ButtonState, MAX_KEY+1> m_KeyStates{};
    std::unordered_set<uint16_t> m_PressedThisFrame;
    std::unordered_set<uint16_t> m_ReleasedThisFrame;

    // Mouse
    std::bitset<8> m_MouseDown; // Left/Right/Middle/4/5…
    std::unordered_set<size_t> m_MousePressedThisFrame;
    std::unordered_set<size_t> m_MouseReleasedThisFrame;
    float m_MouseX = 0.f, m_MouseY = 0.f;
    float m_PrevMouseX = 0.f, m_PrevMouseY = 0.f;
    float m_MouseDX = 0.f, m_MouseDY = 0.f;
    float m_WheelDX = 0.f, m_WheelDY = 0.f;

    // Touch
    std::vector<TouchPoint> m_Touches;
};
