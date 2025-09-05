#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <type_traits>
#include "Event.h"

class EventBus {
public:
  template <typename T> using Handler = std::function<void(const T &)>;

  template <typename T> int Subscribe(Handler<T> cb) {
    auto &vec = getVec<T>();
    std::lock_guard<std::mutex> lock(mutex());
    vec.emplace_back(std::move(cb));
    return int(vec.size()) - 1;
  }

  template <typename T> void Unsubscribe(int id) {
    auto &vec = getVec<T>();
    std::lock_guard<std::mutex> lock(mutex());
    if (id >= 0 && id < (int)vec.size())
      vec[id] = nullptr;
  }

  // Emit helpers
  void Emit(const EWindowResize &e) { call(e, m_OnWindowResize); }
  void Emit(const EContentScale &e) { call(e, m_OnContentScale); }

  void Emit(const EKey &e, bool down) {
    if (down)
      call(e, m_OnKeyDown);
    else
      call(e, m_OnKeyUp);
  }
  void Emit(const EKeyChar &e) { call(e, m_OnKeyChar); }

  void Emit(const EMouseButton &e, bool down) {
    if (down)
      call(e, m_OnMouseDown);
    else
      call(e, m_OnMouseUp);
  }
  void Emit(const EMouseMove &e) { call(e, m_OnMouseMove); }
  void Emit(const EMouseWheel &e) { call(e, m_OnMouseWheel); }

  void EmitClose() { call(dummy, m_OnWindowClose); }

private:
  template <typename T>
    void call(const T& e, std::vector<Handler<T>>& handlers){
    std::lock_guard<std::mutex> lock(mutex());
    for (auto &h : handlers)
      if (h)
	h(e);
  }

  template <typename T> std::vector<Handler<T>> &getVec();

  static std::mutex& mutex() {
    static std::mutex m;
    return m;
  }
private:
  // Per-type handler list
  std::vector<Handler<EWindowResize>> m_OnWindowResize;
  std::vector<Handler<EContentScale>> m_OnContentScale;
  std::vector<Handler<EKey>> m_OnKeyDown, m_OnKeyUp;
  std::vector<Handler<EKeyChar>> m_OnKeyChar;
  std::vector<Handler<EMouseButton>> m_OnMouseDown, m_OnMouseUp;
  std::vector<Handler<EMouseMove>> m_OnMouseMove;
  std::vector<Handler<EMouseWheel>> m_OnMouseWheel;
  std::vector<std::function<void(const int &)>> m_OnWindowClose;
  const int dummy = 0;
};

// Specializations
template<> inline std::vector<EventBus::Handler<EWindowResize>>& EventBus::getVec<EWindowResize>() { return m_OnWindowResize; }
template<> inline std::vector<EventBus::Handler<EContentScale>>& EventBus::getVec<EContentScale>() { return m_OnContentScale; }
template<> inline std::vector<EventBus::Handler<EKey>>&          EventBus::getVec<EKey>()          { return m_OnKeyDown; }  // not used directly
template<> inline std::vector<EventBus::Handler<EKeyChar>>&      EventBus::getVec<EKeyChar>()      { return m_OnKeyChar; }
template<> inline std::vector<EventBus::Handler<EMouseButton>>&  EventBus::getVec<EMouseButton>()  { return m_OnMouseDown; } // not used directly
template<> inline std::vector<EventBus::Handler<EMouseMove>>&    EventBus::getVec<EMouseMove>()    { return m_OnMouseMove; }
template<> inline std::vector<EventBus::Handler<EMouseWheel>>&   EventBus::getVec<EMouseWheel>()   { return m_OnMouseWheel; }
