// dpi_manager.cpp
#include "dpi_manager.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>

DpiManager::DpiManager(GLFWwindow* win)
{
    m_original = ImGui::GetStyle();
    glfwSetWindowContentScaleCallback(win, glfwContentScaleCB);
    float xs, ys; glfwGetWindowContentScale(win, &xs, &ys);
    m_pending = 0.5f * (xs + ys);                     // trigger first rebuild
}

void DpiManager::glfwContentScaleCB(GLFWwindow*, float x, float y)
{
    auto& self = *static_cast<DpiManager*>(ImGui::GetIO().UserData);
    self.m_pending = 0.5f * (x + y);
    self.m_dirty = true;
}

void DpiManager::newFrame(ImGuiIO& io)
{
    if (!m_dirty) return;
    m_dirty = false;
    rebuild(io, m_pending);
}

void DpiManager::rebuild(ImGuiIO& io, float s)
{
    m_scale = s;

    ImGui_ImplOpenGL3_DestroyFontsTexture();
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16.0f * s);
    io.Fonts->Build();
    ImGui_ImplOpenGL3_CreateFontsTexture();

    ImGui::GetStyle() = m_original;
    ImGui::GetStyle().ScaleAllSizes(s);

    io.DisplayFramebufferScale = ImVec2(s, s);
}
