// platform_window.h
#pragma once
#include <GLFW/glfw3.h>

class PlatformWindow {
public:
    PlatformWindow(int width, int height, const char* title);
    ~PlatformWindow();

    bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;
    void makeContextCurrent() const;
    void getFramebufferSize(int& w, int& h) const;
    GLFWwindow* glfw() const { return m_window; }

private:
    static void glfwErrorCallback(int error, const char* desc);
    GLFWwindow* m_window{};
};
