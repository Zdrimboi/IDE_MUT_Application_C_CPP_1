#pragma once

#include <functional>
#include <imgui.h>

class TopBar
{
public:
    // public callbacks you can assign from your app
    std::function<void()> onNewProject;
    std::function<void()> onOpenFile;
    std::function<void()> onSaveAll;
    std::function<void()> onExit;

    std::function<void()> onUndo;
    std::function<void()> onRedo;

    void draw(const char* titleText = "My IDE (v1.4)")
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        // left‑aligned label (disabled menu item style)
        ImGui::TextUnformatted(titleText);
        ImGui::Dummy(ImVec2(20.0f, 0.0f)); // little spacer

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

        ImGui::EndMainMenuBar();
    }
};
