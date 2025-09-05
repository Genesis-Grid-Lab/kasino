#include "input/InputSystem.h"

static inline uint16_t toIndex(Key k){
  auto v = (uint16_t)k;
  return v <= /*InputSystem::MAX_KEY*/ 512 ? v : 0;
}

InputSystem::InputSystem(EventBus& bus) : m_Bus(bus) {
  // init states
  m_KeyStates.fill(ButtonState::Up);

  // Subscribe to events
  // m_Bus.Subscribe<EKey>([this](const EKey& e){
  //   onKey(e, /*down*/true);  // we don't know action from EKey alone, but our emitter calls twice w/ down/up
  // });
  // We want both down and up; add a second subscription that interprets repeat=false and !down as up
  // Simpler: change EventBus to separate OnKeyDown/OnKeyUp channels; but we already have that—use those:
  // m_Bus.Subscribe<EKey>([this](const EKey& e){ /* no-op, placeholder */ });
  // Since EventBus already distinguishes down/up in Emit, wire directly via separate lambdas:
  // m_Bus.Subscribe<EMouseMove>([this](const EMouseMove& e){ onMouseMove(e); });
  // m_Bus.Subscribe<EMouseWheel>([this](const EMouseWheel& e){ onMouseWheel(e); });
  // For mouse button we need both down & up; we’ll rely on two emits from window backend:
  // m_Bus.Subscribe<EMouseButton>([this](const EMouseButton& e){
    // This path is only for "down" if backend calls Emit(..., true). We'll OR both by reading pressed/released sets.
    // Actual down/up distinction is handled in window backend; here we only add edges in handlers.
  // });

  // NOTE:
  // The simplest way is to expose two channels in EventBus: OnMouseDown and
  // OnMouseUp, OnKeyDown and OnKeyUp. We already have that in EventBus
  // (separate vectors), so let's subscribe directly:
  // m_Bus.Subscribe<EWindowResize>([](const EWindowResize&){});
  // Subscribe to the real channels (requires small EventBus additions to expose
  // subscribe helpers) — but to keep current EventBus, we’ll register by
  // calling internal vectors through Emit paths (already separated) via
  // wrappers. See below: we'll attach using lambdas in GlfwWindow when
  // emitting.

  bus.OnKeyDown([this](const EKey& e){ onKey(e, true);  });
  bus.OnKeyUp  ([this](const EKey& e){ onKey(e, false); });
  bus.OnMouseDown([this](const EMouseButton& e){ onMouseButton(e, true);  });
  bus.OnMouseUp  ([this](const EMouseButton& e){ onMouseButton(e, false); });
  bus.Subscribe<EMouseMove>([this](const EMouseMove& e){ onMouseMove(e); });
  bus.Subscribe<EMouseWheel>([this](const EMouseWheel &e) { onMouseWheel(e); });
  //TODO:   
  // bus.Subscribe<ETouch>([this](const ETouch& e){ onTouch(e); });  
}

void InputSystem::BeginFrame() {
  // Clear per-frame edges
  m_PressedThisFrame.clear();
  m_ReleasedThisFrame.clear();
  m_MousePressedThisFrame.clear();
  m_MouseReleasedThisFrame.clear();

  // Reset deltas
  m_PrevMouseX = m_MouseX;
  m_PrevMouseY = m_MouseY;
  m_MouseDX = 0.f;
  m_MouseDY = 0.f;
  m_WheelDX = 0.f;
  m_WheelDY = 0.f;

  // Advance Pressed -> Held, Released -> Up
  for (auto& s : m_KeyStates) {
    if (s == ButtonState::Pressed)  s = ButtonState::Held;
    else if (s == ButtonState::Released) s = ButtonState::Up;
  }
}

ButtonState& InputSystem::stateRef(Key k) {
  return m_KeyStates[toIndex(k)];
}
ButtonState InputSystem::stateOf(Key k) const {
  return m_KeyStates[toIndex(k)];
}

void InputSystem::onKey(const EKey& e, bool down) {
  auto& s = stateRef(e.key);
  if (down) {
    if (s == ButtonState::Up || s == ButtonState::Released) {
      s = ButtonState::Pressed;
      m_PressedThisFrame.insert((uint16_t)e.key);
    } else {
      s = ButtonState::Held;
    }
  } else {
    if (s == ButtonState::Held || s == ButtonState::Pressed) {
      s = ButtonState::Released;
      m_ReleasedThisFrame.insert((uint16_t)e.key);
    } else {
      s = ButtonState::Up;
    }
  }
}

void InputSystem::onMouseButton(const EMouseButton& e, bool down) {
  size_t idx = (size_t)e.button;
  if (down) {
    if (!m_MouseDown.test(idx)) {
      m_MouseDown.set(idx);
      m_MousePressedThisFrame.insert(idx);
    }
  } else {
    if (m_MouseDown.test(idx)) {
      m_MouseDown.reset(idx);
      m_MouseReleasedThisFrame.insert(idx);
    }
  }
  // Position is in e.x/e.y already
  m_MouseX = e.x; m_MouseY = e.y;
}

void InputSystem::onMouseMove(const EMouseMove& e) {
  m_MouseX = e.x; m_MouseY = e.y;
  m_MouseDX += (m_MouseX - m_PrevMouseX);
  m_MouseDY += (m_MouseY - m_PrevMouseY);
  m_PrevMouseX = m_MouseX;
  m_PrevMouseY = m_MouseY;
}

void InputSystem::onMouseWheel(const EMouseWheel& e) {
  m_WheelDX += e.dx;
  m_WheelDY += e.dy;
}

void InputSystem::onTouch(const ETouch& e) {
  m_Touches.assign(e.points, e.points + e.count);
}
