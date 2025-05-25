// ===== text_editor.h =====
#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <future>
#include <atomic>
#include <mutex>
#include "syntax_highlighter.h"
#include "clang_indexer.h"
#include <tree_sitter/api.h>
#include <utility>

// Forward declaration for GetColorForCapture
ImVec4 GetColorForCapture(TokenType type);

struct CursorPosition {
    int line = 0;
    int column = 0;
    bool operator==(const CursorPosition& other) const {
        return line == other.line && column == other.column;
    }
    bool operator!=(const CursorPosition& other) const {
        return !(*this == other);
    }
    bool operator<(const CursorPosition& other) const {
        return line < other.line || (line == other.line && column < other.column);
    }
};

struct EditorState {
    std::string content;
    CursorPosition cursor;
};

// Edit tracking for incremental parsing
struct TextEdit {
    size_t start_byte;
    size_t old_end_byte;
    size_t new_end_byte;
    TSPoint start_point;
    TSPoint old_end_point;
    TSPoint new_end_point;
};

// Line-based caching with update tracking
struct LineCache {
    size_t line_hash = 0;
    std::vector<SyntaxToken> tokens;
    bool is_valid = false;
    bool needs_update = false;  // New field for tracking if update is pending

    void invalidate() {
        is_valid = false;
        needs_update = true;
    }
};

class TextEditor {
public:
    TextEditor(const std::string& file_path, SyntaxHighlighter& highlighter, ClangIndexer& indexer);
    ~TextEditor();
    void Draw();
    const std::string& GetContent() const;
    void SetContent(const std::string& content);

private:
    bool find_case_sensitive_ = false;
    std::optional<float> scrollToLineY_;
    bool show_find_panel_ = false;
    bool find_use_regex_ = false;
    std::string find_query_;
    std::string replace_text_;
    std::vector<CursorPosition> find_results_;
    int current_find_index_ = 0;

    float font_scale_ = 1.0f;  // default scale
    bool deleting_session_ = false;
    std::chrono::steady_clock::time_point last_delete_time_;
    bool scrollToCursor_ = false;
    static constexpr auto TYPING_DEBOUNCE = std::chrono::milliseconds(1000);
    bool    typing_session_ = false;
    std::chrono::steady_clock::time_point last_type_time_;
    int     clickCount_ = 0;
    double  lastClickTime_ = 0.0;

    // Cursor and selection
    CursorPosition cursor_;
    CursorPosition selection_start_;
    bool has_selection_ = false;
    bool is_selecting_with_mouse_ = false;

    // Content state
    std::vector<std::string> lines_;
    mutable std::string cached_content_;
    mutable bool content_dirty_ = true;

    // Edit tracking for incremental updates
    std::vector<TextEdit> pending_edits_;
    std::mutex edit_mutex_;

    // Undo/Redo
    std::vector<EditorState> undo_stack_;
    std::vector<EditorState> redo_stack_;
    static constexpr size_t MAX_UNDO_STACK = 256;

    // External dependencies
    std::string file_path_;
    SyntaxHighlighter& highlighter_;
    ClangIndexer& indexer_;

    // Threading for background processing
    std::future<std::pair<uint64_t, std::vector<SyntaxToken>>> highlight_future_;
    std::atomic<bool> highlight_pending_{ false };
    std::atomic<bool> highlight_dirty_{ false };
    std::future<std::map<std::pair<int, int>, std::string>> semantic_future_;
    std::atomic<bool> semantic_pending_{ false };

    // Token storage with line-based organization
    std::vector<std::vector<SyntaxToken>> tokens_by_line_;
    std::mutex tokens_mutex_;

    // Semantic information
    std::map<std::pair<int, int>, std::string> sem_kind_;
    std::mutex semantic_mutex_;

    // Smart caching
    std::vector<LineCache> line_token_cache_;
    std::unordered_map<size_t, std::vector<SyntaxToken>> token_cache_;
    std::unordered_map<size_t, std::map<std::pair<int, int>, std::string>> semantic_cache_;

    // Timing for debouncing
    std::chrono::steady_clock::time_point last_edit_time_;
    static constexpr auto HIGHLIGHT_DEBOUNCE = std::chrono::milliseconds(0);
    static constexpr auto SEMANTIC_DEBOUNCE = std::chrono::milliseconds(500);

    // Visible area tracking
    int visible_line_start_ = 0;
    int visible_line_count_ = 50;
    float visible_column_start_ = 0;
    float visible_column_width_ = 1000;

    void InsertLineCaches(size_t index, size_t count = 1);
    void EraseLineCaches(size_t index, size_t count = 1);
    std::atomic<uint64_t> content_version_{ 0 };

    // Utility methods
    void SelectWordAt(const CursorPosition& pos);
    void SelectLineAt(int line);
    void UpdateHighlightingAsync();
    void UpdateSemanticKindsAsync();
    void ProcessPendingHighlights();
    void ProcessPendingSemantics();
    void SaveUndo();
    void Undo();
    void Redo();
    void InsertChar(char c);
    void DeleteChar();
    void InsertNewLine();
    void PasteText(const std::string& text);
    void UpdateContentFromLines(int start_line = -1, int end_line = -1);  // Updated signature
    void MoveCursorLeft();
    void MoveCursorRight();
    void MoveCursorUp();
    void MoveCursorDown();
    void ClearSelection() { has_selection_ = false; }
    void SetSelection(const CursorPosition& start) { selection_start_ = start; has_selection_ = true; }
    std::string GetSelectedText();
    void DeleteSelectedText();
    void InsertTextAtCursor(const std::string& text);

    // Optimization helpers
    void CalculateVisibleArea();
    std::vector<SyntaxToken> GetVisibleTokensForLine(int line_number);
    std::vector<SyntaxToken> FilterVisibleTokens(const std::vector<SyntaxToken>& tokens);  // New method
    size_t HashLine(const std::string& line) const;
    size_t HashContent() const;
    void TrackEdit(size_t start_byte, size_t old_length, size_t new_length);
    void RebuildTokensByLine();

    void DrawMinimap();
    void DrawFindReplacePanel();
    bool MatchFind(const std::string& line, int& match_start, int& match_len);
};