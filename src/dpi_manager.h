// dpi_manager.h
#pragma once
#include <GLFW/glfw3.h>
#include <imgui.h>
struct ImGuiIO;

class DpiManager {
public:
    explicit DpiManager(GLFWwindow* win);
    void newFrame(ImGuiIO& io);    // call at the top of every frame
    float scale() const { return m_scale; }
    

private:
    static void glfwContentScaleCB(GLFWwindow*, float x, float y);
    void rebuild(ImGuiIO& io, float s);

    ImGuiStyle m_original;
    float  m_scale = 1.0f;
    float  m_pending = 1.0f;
    bool   m_dirty = true;
};
