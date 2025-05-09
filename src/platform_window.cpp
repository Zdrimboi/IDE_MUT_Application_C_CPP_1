// platform_window.cpp
#include "platform_window.h"
#include <iostream>

PlatformWindow::PlatformWindow(int w, int h, const char* title)
{
    glfwSetErrorCallback(glfwErrorCallback);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    m_window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!m_window) throw std::runtime_error("GLFW window creation failed");
    makeContextCurrent();
    glfwSwapInterval(1);                        // v‑sync
}

PlatformWindow::~PlatformWindow() { if (m_window) glfwDestroyWindow(m_window); }

void PlatformWindow::glfwErrorCallback(int err, const char* desc)
{
    std::cerr << "GLFW error [" << err << "]: " << desc << '\n';
}

bool PlatformWindow::shouldClose() const { return glfwWindowShouldClose(m_window); }
void PlatformWindow::pollEvents() const { glfwPollEvents(); }
void PlatformWindow::swapBuffers() const { glfwSwapBuffers(m_window); }
void PlatformWindow::makeContextCurrent() const { glfwMakeContextCurrent(m_window); }
void PlatformWindow::getFramebufferSize(int& w, int& h) const
{
    glfwGetFramebufferSize(m_window, &w, &h);
}
