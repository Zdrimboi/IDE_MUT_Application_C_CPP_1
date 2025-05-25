#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "text_editor.h"
#include "syntax_highlighter.h"
#include "clang_indexer.h"
#include <text_editor.h>

class EditorWindow {
public:
    EditorWindow();
    ~EditorWindow();
    void Draw();
    void OpenFile(const std::string& path);

private:
    struct EditorTab {
        std::string path;
        std::unique_ptr<TextEditor> editor;
    };

    std::vector<EditorTab> tabs_;
    std::unordered_map<std::string, size_t> path_to_tab_;
    size_t current_tab_ = 0;
    ClangIndexer indexer_;

    std::string DetectLanguage(const std::string& path);
    std::unordered_map<std::string, std::unique_ptr<SyntaxHighlighter>> highlighters_;
};