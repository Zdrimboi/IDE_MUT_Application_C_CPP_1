#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "text_editor.h"
#include "syntax_highlighter.h"
#include "clang_indexer.h"
#include "gui/symbols_panel.h"   // ← new

class EditorWindow
{
public:
    EditorWindow();
    ~EditorWindow();

    /*---------------------  public API  ---------------------*/
    void Draw();
    void OpenFile(const std::string& path);

    /// Link a SymbolsPanel that we will populate and listen to.
    void SetSymbolsPanel(SymbolsPanel* panel);

private:
    /*--------------------  per-tab data  --------------------*/
    struct EditorTab {
        std::string              path;
        std::unique_ptr<TextEditor> editor;
    };

    std::vector<EditorTab>                                tabs_;
    std::unordered_map<std::string, std::size_t>          path_to_tab_;
    std::size_t                                           current_tab_ = 0;

    /*-----------------  infrastructure  --------------------*/
    ClangIndexer                                           indexer_;
    std::unordered_map<std::string,
        std::unique_ptr<SyntaxHighlighter>> highlighters_;

    std::string DetectLanguage(const std::string& path);

    /*------------------  external links  -------------------*/
    static SymbolsPanel* symbols_panel_;   // owned elsewhere
};
