#include "editor_window.h"

#include <filesystem>
#include <fstream>
#include "imgui.h"
#include "gui/symbols_panel.h"

/*──────────────────────────────────────────────────────────*/
/*            static linkage with the Symbols panel         */
SymbolsPanel* EditorWindow::symbols_panel_ = nullptr;
/*──────────────────────────────────────────────────────────*/

EditorWindow::EditorWindow() = default;

EditorWindow::~EditorWindow()
{
    // Global teardown for any libclang state.
    ClangIndexer::Cleanup();
}

/*----------------------------------------------------------*/
/*                external linkage helpers                  */
void EditorWindow::SetSymbolsPanel(SymbolsPanel* panel)
{
    symbols_panel_ = panel;
}
/*----------------------------------------------------------*/

std::string EditorWindow::DetectLanguage(const std::string& path)
{
    auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".c") return "c";
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
        ext == ".hpp" || ext == ".h")
        return "cpp";
    return "unknown";
}

/*----------------------------------------------------------*/
/*                   file-opening logic                     */
void EditorWindow::OpenFile(const std::string& path)
{
    /*—— 1) select existing tab, if any ————————————*/
    if (auto it = path_to_tab_.find(path); it != path_to_tab_.end()) {
        current_tab_ = it->second;
        return;
    }

    /*—— 2) create a brand-new tab ————————————————*/
    const std::string lang = DetectLanguage(path);

    if (!highlighters_.contains(lang))
        highlighters_[lang] = std::make_unique<SyntaxHighlighter>(lang);

    auto editor = std::make_unique<TextEditor>(
        path, *highlighters_[lang], indexer_);

    tabs_.push_back({ path, std::move(editor) });
    path_to_tab_[path] = tabs_.size() - 1;
    current_tab_ = tabs_.size() - 1;

    /*—— 3) index the file & update the Symbols panel ——*/
    if (symbols_panel_)
    {
        /*– gather source code –*/
        std::ifstream ifs(path, std::ios::binary);
        std::string   code((std::istreambuf_iterator<char>(ifs)), {});

        /*– feed panel –*/
        auto symbols = indexer_.Index(path, code);
        symbols_panel_->setSymbols(symbols);

        /*– hook double-click navigation *once* –*/
        symbols_panel_->setActivateCallback(
            [this](int line, int column) {
                if (tabs_.empty()) return;
                /* caret helpers expect 0-based indices */
                tabs_[current_tab_].editor->MoveCursorTo(line - 1, column - 1);
            });
    }
}

/*----------------------------------------------------------*/
/*                      main drawing                        */
void EditorWindow::Draw()
{
    ImGui::Begin("Editor");

    if (ImGui::BeginTabBar("EditorTabs"))
    {
        for (std::size_t i = 0; i < tabs_.size(); ++i)
        {
            bool         open = true;
            const auto   filename = std::filesystem::path(tabs_[i].path)
                .filename()
                .string();

            if (ImGui::BeginTabItem(filename.c_str(), &open))
            {
                current_tab_ = i;

                ImGui::BeginChild("EditorRegion",
                    ImVec2(0, 0),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar);

                tabs_[i].editor->Draw();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            /*—— close-tab housekeeping ————————————*/
            if (!open)
            {
                path_to_tab_.erase(tabs_[i].path);
                tabs_.erase(tabs_.begin() + static_cast<long>(i));

                /* rebuild index map */
                path_to_tab_.clear();
                for (std::size_t j = 0; j < tabs_.size(); ++j)
                    path_to_tab_[tabs_[j].path] = j;

                if (current_tab_ >= tabs_.size())
                    current_tab_ = tabs_.empty() ? 0 : tabs_.size() - 1;
                break;
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
