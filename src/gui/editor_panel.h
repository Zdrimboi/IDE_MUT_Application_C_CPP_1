#pragma once

#include <string>
#include <imgui.h>

class EditorPanel
{
public:
    void setText(std::string txt) { m_buffer = std::move(txt); }
    [[nodiscard]] const std::string& text() const { return m_buffer; }

    void draw(const char* title = "Editor")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        // Allocate a large static buffer for the demo (dynamic grow could be added)
        if (m_buffer.size() < kMaxSize) m_buffer.resize(kMaxSize, '\0');

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CtrlEnterForNewLine;
        ImGui::InputTextMultiline("##source", m_buffer.data(), m_buffer.size(), ImVec2(-FLT_MIN, -FLT_MIN), flags);

        ImGui::End();
    }

private:
    static constexpr size_t kMaxSize = 64 * 1024; // 64‑KB scratch buffer
    std::string m_buffer = "Placeholder source code\nPlaceholder source code\nPlaceholder source code\nPlaceholder source code\nPlaceholder source code\nPlaceholder source code\n";
};
