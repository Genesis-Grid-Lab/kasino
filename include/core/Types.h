#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_set>
#include <optional>
#include <random>
#include <unordered_map>
#include <iterator>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
 //--------------------- Scope = unique pointer --------------------
template<typename T>
using Scope = std::unique_ptr<T>;
template<typename T, typename ... Args>
constexpr Scope<T> CreateScope(Args&& ... args){

  return std::make_unique<T>(std::forward<Args>(args)...);
}

//--------------------- Ref = shared pointer -----------------------
template<typename T>
using Ref = std::shared_ptr<T>;
template<typename T, typename ... Args>
constexpr Ref<T> CreateRef(Args&& ... args){

  return std::make_shared<T>(std::forward<Args>(args)...);
}
