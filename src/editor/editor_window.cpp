#include "editor_window.h"
#include <filesystem>
#include "imgui.h"

EditorWindow::EditorWindow() {}

EditorWindow::~EditorWindow() {
    // Cleanup global resources
    ClangIndexer::Cleanup();
}

std::string EditorWindow::DetectLanguage(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".c") return "c";
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".h") return "cpp";
    return "unkonown"; // fallback
}

void EditorWindow::OpenFile(const std::string& path) {
    auto it = path_to_tab_.find(path);
    if (it != path_to_tab_.end()) {
        current_tab_ = it->second;
        return;
    }
    std::string lang = DetectLanguage(path);

    // Reuse or create highlighter for this language
    if (highlighters_.find(lang) == highlighters_.end())
        highlighters_[lang] = std::make_unique<SyntaxHighlighter>(lang);

    auto editor = std::make_unique<TextEditor>(path, *highlighters_[lang], indexer_);
    tabs_.push_back({ path, std::move(editor) });
    path_to_tab_[path] = tabs_.size() - 1;
    current_tab_ = tabs_.size() - 1;
}

void EditorWindow::Draw() {
    ImGui::Begin("Editor");

    if (ImGui::BeginTabBar("EditorTabs")) {
        for (size_t i = 0; i < tabs_.size(); ++i) {
            bool open = true;
            std::string filename = std::filesystem::path(tabs_[i].path).filename().string();
            if (ImGui::BeginTabItem(filename.c_str(), &open)) {
                current_tab_ = i;

                ImGui::BeginChild("EditorRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                tabs_[i].editor->Draw();
                ImGui::EndChild();

                ImGui::EndTabItem();
            }
            if (!open) {
                path_to_tab_.erase(tabs_[i].path);
                tabs_.erase(tabs_.begin() + i);
                // Rebuild index map
                path_to_tab_.clear();
                for (size_t j = 0; j < tabs_.size(); ++j)
                    path_to_tab_[tabs_[j].path] = j;
                if (current_tab_ >= tabs_.size()) current_tab_ = tabs_.empty() ? 0 : tabs_.size() - 1;
                break;
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}