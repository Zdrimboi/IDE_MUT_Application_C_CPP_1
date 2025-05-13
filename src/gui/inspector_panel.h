#pragma once
#include <imgui.h>
#include <string>

class InspectorPanel
{
public:
    void draw(const char* title = "Inspector")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        ImGui::Text("Object inspector (mock)");
        ImGui::Separator();

        ImGui::InputText("Name", name_, IM_ARRAYSIZE(name_));
        ImGui::DragFloat3("Position", pos_);
        ImGui::DragFloat3("Rotation", rot_);
        ImGui::DragFloat3("Scale", scale_, 0.01f, 0.0f, 10.0f);

        ImGui::End();
    }

private:
    char  name_[64] = "Cube";
    float pos_[3] = { 0.0f, 0.0f, 0.0f };
    float rot_[3] = { 0.0f, 0.0f, 0.0f };
    float scale_[3] = { 1.0f, 1.0f, 1.0f };
};
