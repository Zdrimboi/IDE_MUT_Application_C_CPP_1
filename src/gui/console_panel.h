#pragma once
#include <imgui.h>
#include <deque>
#include <string>

class ConsolePanel
{
public:
    void addLine(std::string msg)          // call from your log system
    {
        lines_.emplace_back(std::move(msg));
        if (lines_.size() > kMaxLines) lines_.pop_front();
    }

    void draw(const char* title = "Console")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }


        if (ImGui::Button("Clear")) lines_.clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto‑scroll", &autoScroll_);
        ImGui::Separator();

        ImGui::BeginChild("##scroll", ImVec2(0, 0), false,
            ImGuiWindowFlags_HorizontalScrollbar);
        for (auto& l : lines_) ImGui::TextUnformatted(l.c_str());
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::End();
    }

private:
    static constexpr size_t kMaxLines = 500;
    std::deque<std::string> lines_ = {
        "[info] Console ready.",
        "[info] Build succeeded (0.123 s)."
    };
    bool autoScroll_ = true;
};
