#pragma once

#include <functional>
#include <imgui.h>
#include <string>
#include <unordered_map>
#include <vector>

class TopBar
{
public:
    std::function<void()> onNewProject;
    std::function<void()> onOpenFile;
    std::function<void()> onSaveAll;
    std::function<void()> onExit;

    std::function<void()> onUndo;
    std::function<void()> onRedo;

    // New: pending dock requests (pop back)
    std::vector<std::pair<std::string, ImGuiID>> pendingRedocks;

    void draw(const std::unordered_map<std::string, ImGuiID>& dockTargets,
        const char* titleText = "My IDE (v1.5)")
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        ImGui::TextUnformatted(titleText);
        ImGui::Dummy(ImVec2(20.0f, 0.0f)); // spacer

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project\tCtrl+Shift+N")) if (onNewProject) onNewProject();
            if (ImGui::MenuItem("Open…\tCtrl+O"))           if (onOpenFile)   onOpenFile();
            ImGui::Separator();
            if (ImGui::MenuItem("Save All\tCtrl+Shift+S")) if (onSaveAll)    onSaveAll();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))                    if (onExit)       onExit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo\tCtrl+Z", nullptr, false, onUndo != nullptr)) if (onUndo) onUndo();
            if (ImGui::MenuItem("Redo\tCtrl+Y", nullptr, false, onRedo != nullptr)) if (onRedo) onRedo();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            for (const auto& [label, dockId] : dockTargets)
            {
                if (ImGui::MenuItem(("Pop back " + label).c_str()))
                    pendingRedocks.emplace_back(label, dockId);
            }

            if (ImGui::MenuItem("Pop back all"))
            {
                for (const auto& [label, dockId] : dockTargets)
                    pendingRedocks.emplace_back(label, dockId);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
};
