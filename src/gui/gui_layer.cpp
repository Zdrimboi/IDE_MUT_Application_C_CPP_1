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

/* ─── window classes ───────────────────────────────────────────────────────── */
static ImGuiWindowClass CLASS_SIDE;
static ImGuiWindowClass CLASS_BOTTOM;
static ImGuiWindowClass CLASS_CENTER;

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

    auto makeClass = [](ImGuiWindowClass& c, const char* tag)
        {
            c = ImGuiWindowClass();
            c.ClassId = ImHashStr(tag);
            c.DockingAllowUnclassed = true; // allow re-docking even if undocked
        };
    makeClass(CLASS_SIDE, "SidePane");
    makeClass(CLASS_BOTTOM, "BottomPane");
    makeClass(CLASS_CENTER, "CenterPane");

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
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
        vp->ID, vp,
        ImGuiDockNodeFlags_PassthruCentralNode |
        ImGuiDockNodeFlags_NoDockingInCentralNode);

    static bool first = true;
    static ImGuiID nodeL = 0, nodeR = 0, nodeB = 0, nodeC = 0;

    if (first)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->Size);

        nodeL = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, nullptr, &dockspace_id);
        nodeB = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.28f, nullptr, &nodeC);
        nodeR = ImGui::DockBuilderSplitNode(nodeC, ImGuiDir_Right, 0.25f, nullptr, &nodeC);

        // Lock center node for the editor
        ImGuiDockNode* centerNode = ImGui::DockBuilderGetNode(nodeC);
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoSplit;

        panelDockTargets = {
            { "File Manager", nodeL },
            { "Inspector",    nodeR },
            { "Symbols",      nodeR },
            { "Console",      nodeB },
            { "Editor",       nodeC }
        };

        ImGui::DockBuilderDockWindow("File Manager", nodeL);
        ImGui::DockBuilderDockWindow("Inspector", nodeR);
        ImGui::DockBuilderDockWindow("Symbols", nodeR);
        ImGui::DockBuilderDockWindow("Console", nodeB);
        ImGui::DockBuilderDockWindow("Editor", nodeC);

        ImGui::DockBuilderFinish(dockspace_id);
        first = false;
    }

    // Assign window classes before rendering
    ImGui::SetNextWindowClass(&CLASS_SIDE);    fm.draw("File Manager");
    ImGui::SetNextWindowClass(&CLASS_SIDE);    symbols.draw("Symbols");
    ImGui::SetNextWindowClass(&CLASS_SIDE);    inspector.draw("Inspector");
    ImGui::SetNextWindowClass(&CLASS_BOTTOM);  console.draw("Console");
    ImGui::SetNextWindowClass(&CLASS_CENTER);  editor.draw();

    topBar.draw(panelDockTargets);

    for (const auto& [label, dockId] : topBar.pendingRedocks)
    {
        if (!ImGui::DockBuilderGetNode(dockId))
        {
            // Recreate the node and restore it to the hierarchy
            ImGuiID root = ImGui::GetMainViewport()->ID;

            // Determine direction based on original target
            if (dockId == panelDockTargets["File Manager"]) {
                ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.22f, &panelDockTargets["File Manager"], &root);
            }
            else if (dockId == panelDockTargets["Console"]) {
                ImGui::DockBuilderSplitNode(root, ImGuiDir_Down, 0.28f, &panelDockTargets["Console"], &root);
            }
            else if (dockId == panelDockTargets["Inspector"] || dockId == panelDockTargets["Symbols"]) {
                ImGui::DockBuilderSplitNode(root, ImGuiDir_Right, 0.25f, &panelDockTargets["Inspector"], &root);
                panelDockTargets["Symbols"] = panelDockTargets["Inspector"]; // Share nodeR
            }
            // Do not touch "Editor" — user requested it be fixed

            ImGui::DockBuilderFinish(ImGui::GetMainViewport()->ID);
        }

        ImGui::DockBuilderDockWindow(label.c_str(), dockId);
    }
    topBar.pendingRedocks.clear();


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
