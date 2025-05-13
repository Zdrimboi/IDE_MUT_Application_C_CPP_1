#pragma once
#include <imgui.h>
#include <vector>
#include <string>

class SymbolsPanel
{
public:
    // In a real tool you’d call setSymbols() from your compiler/parser
    void setSymbols(std::vector<std::string> list) { symbols_ = std::move(list); }

    void draw(const char* title = "Symbols")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        ImGui::Text("Project symbols (mock)");
        ImGui::Separator();
        for (auto& s : symbols_)
            ImGui::BulletText("%s", s.c_str());

        ImGui::End();
    }

private:
    std::vector<std::string> symbols_ = {
        "main()", "Render()", "App::Init()", "_update()", "Player::Move()" };
};
