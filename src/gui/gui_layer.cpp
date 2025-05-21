// ─── gui_layer.cpp ────────────────────────────────────────────────────────────
#include <glfw/glfw3.h>
#include "gui_layer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <gui/filemanager_panel.h>
#include <gui/editor_panel.h>
#include <gui/top_bar.h>
#include <gui/symbols_panel.h>
#include <gui/inspector_panel.h>
#include <gui/console_panel.h>

namespace fs = std::filesystem;

/* ─── panels ───────────────────────────────────────────────────────────────── */
FileManagerPanel fm{ fs::current_path() };
EditorPanel      editor;
TopBar           topBar{fm};
SymbolsPanel     symbols;
InspectorPanel   inspector;
ConsolePanel     console;

/* ─── dock node tracking ───────────────────────────────────────────────────── */
static std::unordered_map<std::string, ImGuiID> panelDockTargets;

void GuiLayer::init(void* win)
{
    fm.setOpenFileCallback([&](const fs::path& p)
        {
            std::ifstream ifs(p, std::ios::binary);
            std::string txt((std::istreambuf_iterator<char>(ifs)),
                std::istreambuf_iterator<char>());
            editor.openFile(p);
        });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
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
    // 1) full-screen host window
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##MainHost", nullptr, host_flags);
    ImGui::PopStyleVar(2);

    // 2) one-time DockBuilder layout
    static bool   dock_setup = false;
    static float  left_ratio = 0.20f;  // 20% of width for File Manager
    static float  bottom_ratio = 0.30f;  // bottom 30% of height for Editor+Symbols
    static float  right_ratio = 0.25f;  // right 25% of that bottom stripe for Symbols/Inspector
    // 2) one-time DockBuilder layout (swap Console & Editor + move Symbols/Inspector above Console)
    if (!dock_setup)
    {
        dock_setup = true;

        ImGuiID dock_id = ImGui::GetID("MainDockSpace");
        ImGui::DockBuilderRemoveNode(dock_id);
        ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_None);
        ImGui::DockBuilderSetNodeSize(dock_id, vp->WorkSize);

        // a) split root → left (File Manager) + right (everything else)
        ImGuiID id_fileMgr, id_right;
        ImGui::DockBuilderSplitNode(dock_id, ImGuiDir_Left, left_ratio, &id_fileMgr, &id_right);

        // b) split right → top (Editor+Symbols) + bottom (Console)
        ImGuiID id_console, id_top;
        ImGui::DockBuilderSplitNode(id_right, ImGuiDir_Down, bottom_ratio, &id_console, &id_top);

        // c) split the top region → left (Editor) + right (Symbols/Inspector)
        ImGuiID id_symbols, id_editor;
        ImGui::DockBuilderSplitNode(id_top, ImGuiDir_Right, right_ratio, &id_symbols, &id_editor);

        // d) dock your windows into each node
        ImGui::DockBuilderDockWindow("File Manager", id_fileMgr);
        ImGui::DockBuilderDockWindow("Editor", id_editor);
        ImGui::DockBuilderDockWindow("Console", id_console);
        ImGui::DockBuilderDockWindow("Symbols", id_symbols);
        ImGui::DockBuilderDockWindow("Inspector", id_symbols);

        ImGui::DockBuilderFinish(dock_id);
    }


    // 3) the actual dockspace
    ImGui::DockSpace(
        ImGui::GetID("MainDockSpace"),
        ImVec2(0, 0),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    // 4) draw your panels exactly as before
    fm.draw("File Manager");
    console.draw("Console");
    editor.draw("Editor");
    symbols.draw("Symbols");
    inspector.draw("Inspector");
    topBar.draw(panelDockTargets, "MUT Demo (v1.5)");

    ImGui::End();
}




void GuiLayer::end()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup = glfwGetCurrentContext();
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
