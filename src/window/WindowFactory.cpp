#include "window/WindowFactory.h"

#if defined(WINDOW_BACKEND_GLFW)
    #include "window/glfw/GlfwWindow.h"
#elif defined(WINDOW_BACKEND_NATIVE)
    #include "window/native/NativeWindow.h"
#endif

std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc){
#if defined(WINDOW_BACKEND_GLFW)
    return std::make_unique<GlfwWindow>(desc);
#elif defined(WINDOW_BACKEND_NATIVE)
    return std::make_unique<NativeWindow>(desc);
#else
    static_assert(false, "No window backend selected")
    return {};
#endif
}