// gui_layer.cpp
#include <glfw/glfw3.h>
#include "gui_layer.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <gui/filemanager_panel.h>
#include <gui/top_bar.h>
#include <gui/editor_panel.h>

FileExplorer explorer{ std::filesystem::current_path() };
TopBar topBar;
EditorPanel editorPanel;

void GuiLayer::init(void* win)
{
    explorer.setRootPath("C:/");

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(win), true);
    ImGui_ImplOpenGL3_Init();
}

void GuiLayer::begin()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GuiLayer::render()
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);
    explorer.draw();
	topBar.draw();
	editorPanel.draw();
}

void GuiLayer::end()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        auto* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}

void GuiLayer::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

