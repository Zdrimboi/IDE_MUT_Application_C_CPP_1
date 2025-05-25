#include "text_editor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <numeric>
#include <cctype>
#include "imgui.h"
#include "imgui_internal.h"
#include <regex>

//#define DEBUG_TEXTEDITOR

#ifdef DEBUG_TEXTEDITOR
#include <cstdio>
#include <cstdarg>

// ANSI color codes for terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\x1b[1m"

// Debug modules
enum class DebugModule {
    CORE,         // Core operations (constructor, destructor, etc.)
    EDIT,         // Text editing operations
    CURSOR,       // Cursor movement
    SELECTION,    // Selection operations
    CLIPBOARD,    // Copy/paste operations
    UNDO,         // Undo/redo operations
    HIGHLIGHT,    // Syntax highlighting
    SEMANTIC,     // Semantic analysis
    CACHE,        // Cache operations
    RENDER,       // Rendering operations
    SEARCH,       // Find/replace operations
    MOUSE,        // Mouse operations
    KEYBOARD,     // Keyboard input
    MINIMAP,      // Minimap operations
    SCROLL,       // Scrolling operations
    PERF          // Performance metrics
};

const char* GetModuleName(DebugModule module) {
    switch (module) {
    case DebugModule::CORE:      return "CORE";
    case DebugModule::EDIT:      return "EDIT";
    case DebugModule::CURSOR:    return "CURSOR";
    case DebugModule::SELECTION: return "SELECTION";
    case DebugModule::CLIPBOARD: return "CLIPBOARD";
    case DebugModule::UNDO:      return "UNDO";
    case DebugModule::HIGHLIGHT: return "HIGHLIGHT";
    case DebugModule::SEMANTIC:  return "SEMANTIC";
    case DebugModule::CACHE:     return "CACHE";
    case DebugModule::RENDER:    return "RENDER";
    case DebugModule::SEARCH:    return "SEARCH";
    case DebugModule::MOUSE:     return "MOUSE";
    case DebugModule::KEYBOARD:  return "KEYBOARD";
    case DebugModule::MINIMAP:   return "MINIMAP";
    case DebugModule::SCROLL:    return "SCROLL";
    case DebugModule::PERF:      return "PERF";
    default:                     return "UNKNOWN";
    }
}

const char* GetModuleColor(DebugModule module) {
    switch (module) {
    case DebugModule::CORE:      return ANSI_COLOR_BLUE;
    case DebugModule::EDIT:      return ANSI_COLOR_GREEN;
    case DebugModule::CURSOR:    return ANSI_COLOR_CYAN;
    case DebugModule::SELECTION: return ANSI_COLOR_MAGENTA;
    case DebugModule::CLIPBOARD: return ANSI_COLOR_YELLOW;
    case DebugModule::UNDO:      return ANSI_COLOR_RED;
    case DebugModule::HIGHLIGHT: return ANSI_COLOR_GREEN;
    case DebugModule::SEMANTIC:  return ANSI_COLOR_BLUE;
    case DebugModule::CACHE:     return ANSI_COLOR_MAGENTA;
    case DebugModule::RENDER:    return ANSI_COLOR_CYAN;
    case DebugModule::SEARCH:    return ANSI_COLOR_YELLOW;
    case DebugModule::MOUSE:     return ANSI_COLOR_RED;
    case DebugModule::KEYBOARD:  return ANSI_COLOR_GREEN;
    case DebugModule::MINIMAP:   return ANSI_COLOR_BLUE;
    case DebugModule::SCROLL:    return ANSI_COLOR_MAGENTA;
    case DebugModule::PERF:      return ANSI_COLOR_YELLOW;
    default:                     return ANSI_COLOR_RESET;
    }
}

void DebugPrint(DebugModule module, const char* action, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Print timestamp
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* tm = localtime(&time_t);

    fprintf(stderr, "[%02d:%02d:%02d.%03d] ",
        tm->tm_hour, tm->tm_min, tm->tm_sec, (int)ms.count());

    // Print module with color
    fprintf(stderr, "%s%s[%-10s]%s ",
        ANSI_COLOR_BOLD, GetModuleColor(module),
        GetModuleName(module), ANSI_COLOR_RESET);

    // Print action
    fprintf(stderr, "%s%-20s%s ", ANSI_COLOR_BOLD, action, ANSI_COLOR_RESET);

    // Print details
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

#define DBG_TEDITOR(module, action, fmt, ...) \
    DebugPrint(module, action, fmt, ##__VA_ARGS__)
#else
#define DBG_TEDITOR(module, action, fmt, ...) ((void)0)
#endif

static std::string SafeSubstr(const std::string& s, int pos, int count = INT_MAX)
{
    if (pos < 0 || pos >= (int)s.size())
        return "";
    // clamp count so pos+count ≤ s.size()
    int maxCount = std::min(count, (int)s.size() - pos);
    return s.substr(pos, maxCount);
}

TextEditor::TextEditor(const std::string& file_path, SyntaxHighlighter& highlighter, ClangIndexer& indexer)
    : file_path_(file_path), highlighter_(highlighter), indexer_(indexer)
{
    DBG_TEDITOR(DebugModule::CORE, "Constructor", "Initializing TextEditor for file: %s", file_path.c_str());

    std::ifstream in(file_path_);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    DBG_TEDITOR(DebugModule::CORE, "FileLoad", "Loaded %zu bytes from file", content.size());

    // Pre-allocate based on estimated line count
    size_t estimated_lines = std::count(content.begin(), content.end(), '\n') + 1;
    lines_.reserve(estimated_lines);

    DBG_TEDITOR(DebugModule::CORE, "Memory", "Pre-allocated space for %zu lines", estimated_lines);

    // Split content into lines efficiently
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines_.push_back(std::move(line));
    }
    if (lines_.empty()) lines_.push_back("");

    DBG_TEDITOR(DebugModule::CORE, "Parse", "Parsed %zu lines from content", lines_.size());

    cursor_ = { 0, 0 };

    // Initialize caches
    line_token_cache_.resize(lines_.size());
    tokens_by_line_.resize(lines_.size());

    DBG_TEDITOR(DebugModule::CACHE, "Init", "Initialized caches for %zu lines", lines_.size());

    // Start background processing
    UpdateHighlightingAsync();
    UpdateSemanticKindsAsync();

    DBG_TEDITOR(DebugModule::CORE, "Constructor", "TextEditor initialization complete");
}

TextEditor::~TextEditor() {
    DBG_TEDITOR(DebugModule::CORE, "Destructor", "Cleaning up TextEditor");

    // Wait for any pending async operations
    if (highlight_future_.valid()) {
        DBG_TEDITOR(DebugModule::HIGHLIGHT, "Cleanup", "Waiting for pending highlight task");
        highlight_future_.wait();
    }
    if (semantic_future_.valid()) {
        DBG_TEDITOR(DebugModule::SEMANTIC, "Cleanup", "Waiting for pending semantic task");
        semantic_future_.wait();
    }

    DBG_TEDITOR(DebugModule::CORE, "Destructor", "TextEditor cleanup complete");
}

void TextEditor::InsertLineCaches(size_t idx, size_t n) {
    DBG_TEDITOR(DebugModule::CACHE, "InsertLines", "Inserting %zu cache entries at index %zu", n, idx);

    line_token_cache_.insert(line_token_cache_.begin() + idx, n, {});
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    tokens_by_line_.insert(tokens_by_line_.begin() + idx, n, {});
}

void TextEditor::EraseLineCaches(size_t idx, size_t n) {
    DBG_TEDITOR(DebugModule::CACHE, "EraseLines", "Erasing %zu cache entries from index %zu", n, idx);

    line_token_cache_.erase(line_token_cache_.begin() + idx,
        line_token_cache_.begin() + idx + n);
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    tokens_by_line_.erase(tokens_by_line_.begin() + idx,
        tokens_by_line_.begin() + idx + n);
}

bool TextEditor::MatchFind(const std::string& line, int& match_start, int& match_len) {
    match_start = 0;
    match_len = 0;

    if (find_query_.empty()) {
        DBG_TEDITOR(DebugModule::SEARCH, "Match", "Empty search query, returning false");
        return false;
    }

    try {
        if (find_use_regex_) {
            DBG_TEDITOR(DebugModule::SEARCH, "RegexMatch", "Attempting regex match for: %s", find_query_.c_str());

            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;

            // Only case-insensitive toggle
            if (!find_case_sensitive_)
                flags |= std::regex_constants::icase;

            std::regex rgx(find_query_, flags);
            std::smatch match;

            if (std::regex_search(line, match, rgx) && match.ready() && !match.empty()) {
                match_start = static_cast<int>(match.position(0));
                match_len = static_cast<int>(match.length(0));
                DBG_TEDITOR(DebugModule::SEARCH, "RegexMatch", "Found match at pos %d, len %d", match_start, match_len);
                return true;
            }
        }
        else {
            DBG_TEDITOR(DebugModule::SEARCH, "StringMatch", "Attempting string match for: %s", find_query_.c_str());

            std::string haystack = line;
            std::string needle = find_query_;

            if (!find_case_sensitive_) {
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
                std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
            }

            size_t pos = haystack.find(needle);
            if (pos != std::string::npos) {
                match_start = static_cast<int>(pos);
                match_len = static_cast<int>(needle.length());
                DBG_TEDITOR(DebugModule::SEARCH, "StringMatch", "Found match at pos %d, len %d", match_start, match_len);
                return true;
            }
        }
    }
    catch (const std::regex_error& e) {
        DBG_TEDITOR(DebugModule::SEARCH, "RegexError", "Invalid regex: %s", e.what());
    }

    return false;
}

void TextEditor::SetContent(const std::string& content)
{
    DBG_TEDITOR(DebugModule::EDIT, "SetContent", "Setting new content, size=%zu bytes", content.size());

    // 1.  Read new buffer into temporary vector
    std::vector<std::string> new_lines;
    std::istringstream       iss(content);
    std::string              line;
    while (std::getline(iss, line))
        new_lines.push_back(std::move(line));
    if (new_lines.empty())
        new_lines.push_back("");

    const size_t old_size = lines_.size();
    const size_t new_size = new_lines.size();

    DBG_TEDITOR(DebugModule::EDIT, "SetContent", "Old lines: %zu, New lines: %zu", old_size, new_size);

    
    // 2.  Longest common prefix / suffix detection
    size_t prefix_len = 0;
    while (prefix_len < old_size && prefix_len < new_size &&
        lines_[prefix_len] == new_lines[prefix_len])
        ++prefix_len;

    size_t suffix_len = 0;
    while (suffix_len < old_size - prefix_len &&
        suffix_len < new_size - prefix_len &&
        lines_[old_size - 1 - suffix_len] ==
        new_lines[new_size - 1 - suffix_len])
        ++suffix_len;

    DBG_TEDITOR(DebugModule::PERF, "Diff", "Common prefix: %zu lines, suffix: %zu lines", prefix_len, suffix_len);
    DBG_TEDITOR(DebugModule::PERF, "Diff", "Changed range: lines %zu to %zu", prefix_len, new_size - suffix_len - 1);

    
    // 3.  Build caches for new buffer (reuse unchanged lines)
    std::vector<LineCache>                 new_line_caches(new_size);
    std::vector<std::vector<SyntaxToken>>  new_tokens_by_line(new_size);

    {
        std::lock_guard<std::mutex> lock(tokens_mutex_);

        // copy unchanged prefix
        for (size_t i = 0; i < prefix_len; ++i) {
            new_line_caches[i] = line_token_cache_[i];
            new_tokens_by_line[i] = tokens_by_line_[i];
        }

        DBG_TEDITOR(DebugModule::CACHE, "Reuse", "Reused %zu prefix cache entries", prefix_len);

        // copy unchanged suffix (may be shifted)
        const ptrdiff_t diff = static_cast<ptrdiff_t>(old_size) -
            static_cast<ptrdiff_t>(new_size);
        for (size_t n = 0; n < suffix_len; ++n) {
            size_t new_idx = new_size - 1 - n;
            size_t old_idx = new_idx + diff;
            new_line_caches[new_idx] = line_token_cache_[old_idx];
            new_tokens_by_line[new_idx] = tokens_by_line_[old_idx];
        }

        DBG_TEDITOR(DebugModule::CACHE, "Reuse", "Reused %zu suffix cache entries", suffix_len);
    }

    // invalidate the genuinely changed middle block
    size_t invalidated_count = 0;
    for (size_t i = prefix_len; i < new_size - suffix_len; ++i) {
        new_line_caches[i].is_valid = false;
        new_line_caches[i].needs_update = true;
        invalidated_count++;
    }

    DBG_TEDITOR(DebugModule::CACHE, "Invalidate", "Invalidated %zu cache entries", invalidated_count);

    
    // 4.  Swap new state into place
    {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        tokens_by_line_.swap(new_tokens_by_line);
    }
    line_token_cache_.swap(new_line_caches);
    lines_.swap(new_lines);

    
    // 5.  Reset cursor / selection and queue incremental highlight
    cursor_ = { 0, 0 };
    has_selection_ = false;

    DBG_TEDITOR(DebugModule::CURSOR, "Reset", "Cursor reset to (0, 0)");

    if (prefix_len < new_size - suffix_len) {
        int first_changed = static_cast<int>(prefix_len);
        int last_changed = static_cast<int>(new_size - suffix_len - 1);
        UpdateContentFromLines(first_changed, last_changed);
    }

    DBG_TEDITOR(DebugModule::EDIT, "SetContent", "Content update complete");
}

size_t TextEditor::HashLine(const std::string& line) const {
    size_t hash = std::hash<std::string>{}(line);
    //G_TEDITOR(DebugModule::CACHE, "HashLine", "Line hash: %zx, length: %zu", hash, line.length());
    return hash;
}

size_t TextEditor::HashContent() const {
    size_t hash = 0;
    for (const auto& line : lines_) {
        hash ^= HashLine(line) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    DBG_TEDITOR(DebugModule::CACHE, "HashContent", "Content hash: %zx for %zu lines", hash, lines_.size());
    return hash;
}

void TextEditor::TrackEdit(size_t start_byte, size_t old_length, size_t new_length) {
    DBG_TEDITOR(DebugModule::EDIT, "TrackEdit", "Tracking edit at byte %zu: old_len=%zu, new_len=%zu",
        start_byte, old_length, new_length);

    std::lock_guard<std::mutex> lock(edit_mutex_);

    size_t byte_pos = 0;
    int line = 0;
    int column = 0;

    for (int i = 0; i < lines_.size() && byte_pos < start_byte; ++i) {
        size_t line_length = lines_[i].length() + 1;
        if (byte_pos + line_length > start_byte) {
            line = i;
            column = start_byte - byte_pos;
            break;
        }
        byte_pos += line_length;
    }

    TextEdit edit;
    edit.start_byte = start_byte;
    edit.old_end_byte = start_byte + old_length;
    edit.new_end_byte = start_byte + new_length;
    edit.start_point = { static_cast<uint32_t>(line), static_cast<uint32_t>(column) };

    pending_edits_.push_back(edit);

    DBG_TEDITOR(DebugModule::EDIT, "TrackEdit", "Edit tracked at line %d, column %d", line, column);
}

const std::string& TextEditor::GetContent() const {
    if (content_dirty_) {
        DBG_TEDITOR(DebugModule::CACHE, "GetContent", "Rebuilding content cache");

        cached_content_.clear();
        cached_content_.reserve(std::accumulate(lines_.begin(), lines_.end(), size_t(0),
            [](size_t sum, const std::string& line) { return sum + line.size() + 1; }));

        for (size_t i = 0; i < lines_.size(); ++i) {
            cached_content_ += lines_[i];
            if (i + 1 < lines_.size()) cached_content_ += '\n';
        }
        content_dirty_ = false;

        DBG_TEDITOR(DebugModule::CACHE, "GetContent", "Content cache rebuilt: %zu bytes", cached_content_.size());
    }
    return cached_content_;
}

void TextEditor::UpdateHighlightingAsync()
{
    // If a highlight job is already in flight, skip queuing another.
    if (highlight_pending_.exchange(true)) {
        DBG_TEDITOR(DebugModule::HIGHLIGHT, "Async", "Highlight already pending, skipping");
        return;
    }

    uint64_t this_version = content_version_.load();
    DBG_TEDITOR(DebugModule::HIGHLIGHT, "AsyncStart",
        "Launching async highlight task, version=%llu",
        static_cast<unsigned long long>(this_version));

    // Grab a snapshot of the current content and edits
    std::string           content = GetContent();
    std::vector<TextEdit> edits;
    {
        std::lock_guard<std::mutex> lock(edit_mutex_);
        edits = std::move(pending_edits_);
    }

    DBG_TEDITOR(DebugModule::HIGHLIGHT, "AsyncStart",
        "Highlighting %zu bytes with %zu pending edits",
        content.size(), edits.size());

    // Launch background task
    highlight_future_ = std::async(
        std::launch::async,
        [this,
        content = std::move(content),
        edits = std::move(edits),
        this_version]() -> std::pair<uint64_t, std::vector<SyntaxToken>>
        {
            // If we have edits, skip the global cache entirely
            if (!edits.empty()) {
                DBG_TEDITOR(DebugModule::CACHE, "TokenCache",
                    "Skipping cache lookup due to %zu pending edits", edits.size());
                auto tokens = highlighter_.HighlightIncremental(content, edits);
                DBG_TEDITOR(DebugModule::HIGHLIGHT, "AsyncProcess",
                    "Generated %zu tokens", tokens.size());
                return { this_version, std::move(tokens) };
            }

            // No edits: attempt to hit the cache
            size_t h = std::hash<std::string>{}(content);
            if (auto it = token_cache_.find(h); it != token_cache_.end()) {
                DBG_TEDITOR(DebugModule::CACHE, "TokenCache",
                    "Cache HIT for hash %zx: %zu tokens", h, it->second.size());
                return { this_version, it->second };
            }

            // Cache miss: do a full incremental highlight and insert into cache
            DBG_TEDITOR(DebugModule::CACHE, "TokenCache",
                "Cache MISS for hash %zx, highlighting.", h);
            auto tokens = highlighter_.HighlightIncremental(content, edits);
            DBG_TEDITOR(DebugModule::HIGHLIGHT, "AsyncProcess",
                "Generated %zu tokens", tokens.size());

            token_cache_[h] = tokens;
            if (token_cache_.size() > 10) {
                DBG_TEDITOR(DebugModule::CACHE, "TokenCache",
                    "Cache size exceeded limit, clearing");
                token_cache_.clear();
                token_cache_[h] = tokens;
            }
            
            return { this_version, std::move(tokens) };
        });
}


void TextEditor::UpdateSemanticKindsAsync() {
    if (semantic_pending_.exchange(true)) {
        DBG_TEDITOR(DebugModule::SEMANTIC, "Async", "Semantic analysis already pending, skipping");
        return;
    }

    DBG_TEDITOR(DebugModule::SEMANTIC, "AsyncStart", "Launching async semantic analysis");

    std::string content = GetContent();

    semantic_future_ = std::async(std::launch::async, [this, content = std::move(content)]() {
        size_t content_hash = std::hash<std::string>{}(content);

        auto cache_it = semantic_cache_.find(content_hash);
        if (cache_it != semantic_cache_.end()) {
            DBG_TEDITOR(DebugModule::CACHE, "SemanticCache", "Cache HIT for hash %zx", content_hash);
            return cache_it->second;
        }

        DBG_TEDITOR(DebugModule::CACHE, "SemanticCache", "Cache MISS for hash %zx, indexing...", content_hash);

        auto symbols = indexer_.Index(file_path_, content);
        std::map<std::pair<int, int>, std::string> sem_kind;

        DBG_TEDITOR(DebugModule::SEMANTIC, "AsyncProcess", "Indexed %zu symbols", symbols.size());

        for (const auto& sym : symbols) {
            sem_kind[{sym.line, sym.column}] = sym.kind;
        }

        semantic_cache_[content_hash] = sem_kind;
        if (semantic_cache_.size() > 5) {
            DBG_TEDITOR(DebugModule::CACHE, "SemanticCache", "Cache size exceeded limit, clearing");
            semantic_cache_.clear();
            semantic_cache_[content_hash] = sem_kind;
        }

        return sem_kind;
        });
}

void TextEditor::ProcessPendingHighlights()
{
    if (highlight_future_.valid() &&
        highlight_future_.wait_for(std::chrono::milliseconds(0))
        == std::future_status::ready)
    {
        DBG_TEDITOR(DebugModule::HIGHLIGHT, "Process", "Highlight result ready");

        auto [job_ver, tokens] = highlight_future_.get();
        highlight_pending_ = false;

        if (job_ver != content_version_.load()) {
            DBG_TEDITOR(DebugModule::HIGHLIGHT, "StaleResult",
                "Discarding stale result (job v%llu != current v%llu)",
                static_cast<unsigned long long>(job_ver),
                static_cast<unsigned long long>(content_version_.load()));

            if (highlight_dirty_.exchange(false))
                UpdateHighlightingAsync();
            return;
        }

        DBG_TEDITOR(DebugModule::HIGHLIGHT, "Apply", "Applying %zu tokens", tokens.size());

        size_t h = HashContent();
        token_cache_[h] = tokens;

        RebuildTokensByLine();
        for (auto& c : line_token_cache_)
            c.needs_update = true;

        if (highlight_dirty_.exchange(false)) {
            DBG_TEDITOR(DebugModule::HIGHLIGHT, "DirtyFlag", "Dirty flag was set, queuing follow-up");
            UpdateHighlightingAsync();
        }
    }
}

void TextEditor::ProcessPendingSemantics() {
    if (semantic_future_.valid() &&
        semantic_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {

        DBG_TEDITOR(DebugModule::SEMANTIC, "Process", "Semantic result ready");

        std::lock_guard<std::mutex> lock(semantic_mutex_);
        sem_kind_ = semantic_future_.get();
        semantic_pending_ = false;

        DBG_TEDITOR(DebugModule::SEMANTIC, "Apply", "Applied %zu semantic kinds", sem_kind_.size());
    }
}

void TextEditor::RebuildTokensByLine() {
    DBG_TEDITOR(DebugModule::HIGHLIGHT, "RebuildLines", "Rebuilding tokens for %zu lines", lines_.size());

    std::lock_guard<std::mutex> lock(tokens_mutex_);

    // Store old tokens for fallback
    auto old_tokens_by_line = tokens_by_line_;

    // Clear and resize
    tokens_by_line_.clear();
    tokens_by_line_.resize(lines_.size());

    size_t content_hash = HashContent();
    auto cache_it = token_cache_.find(content_hash);
    if (cache_it == token_cache_.end()) {
        DBG_TEDITOR(DebugModule::HIGHLIGHT, "RebuildLines", "No tokens found, keeping old tokens");
        tokens_by_line_ = old_tokens_by_line;
        return;
    }

    size_t token_count = 0;
    // Distribute new tokens to lines
    for (const auto& token : cache_it->second) {
        int line_idx = token.line - 1;
        if (line_idx >= 0 && line_idx < tokens_by_line_.size()) {
            tokens_by_line_[line_idx].push_back(token);
            token_count++;
        }
    }

    DBG_TEDITOR(DebugModule::HIGHLIGHT, "RebuildLines", "Distributed %zu tokens to lines", token_count);

    // Sort tokens by column within each line
    for (size_t i = 0; i < tokens_by_line_.size(); ++i) {
        auto& line_tokens = tokens_by_line_[i];
        if (!line_tokens.empty()) {
            std::sort(line_tokens.begin(), line_tokens.end(),
                [](const auto& a, const auto& b) { return a.column < b.column; });

            DBG_TEDITOR(DebugModule::HIGHLIGHT, "SortTokens", "Line %zu: %zu tokens sorted",
                i, line_tokens.size());
        }
    }
}

std::vector<SyntaxToken> TextEditor::GetVisibleTokensForLine(int line_number) {
    if (line_number < 0 || line_number >= lines_.size()) {
        DBG_TEDITOR(DebugModule::RENDER, "GetTokens", "Invalid line number: %d", line_number);
        return {};
    }

    auto& cache = line_token_cache_[line_number];
    size_t line_hash = HashLine(lines_[line_number]);

    // Check if cache is valid and doesn't need update
    if (cache.is_valid && !cache.needs_update && cache.line_hash == line_hash) {
        //G_TEDITOR(DebugModule::CACHE, "LineCache", "Cache HIT for line %d", line_number);
        return FilterVisibleTokens(cache.tokens);
    }

    //DBG_TEDITOR(DebugModule::CACHE, "LineCache", "Cache MISS for line %d, updating...", line_number);

    // Update cache from tokens_by_line
    {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        if (line_number < tokens_by_line_.size()) {
            // If we have new tokens, use them
            if (!tokens_by_line_[line_number].empty()) {
                cache.tokens = tokens_by_line_[line_number];
                cache.line_hash = line_hash;
                cache.is_valid = true;
                cache.needs_update = false;
                DBG_TEDITOR(DebugModule::CACHE, "LineCache", "Updated line %d with %zu tokens",
                    line_number, cache.tokens.size());
            }
            // If no new tokens but cache is invalid, create default tokens
            else if (!cache.is_valid) {
                // Create a single default token for the entire line
                cache.tokens.clear();
                if (!lines_[line_number].empty()) {
                    cache.tokens.push_back({
                        line_number + 1,
                        0,
                        static_cast<int>(lines_[line_number].length()),
                        TokenType::Default,
                        GetColorForCapture(TokenType::Default)
                        });
                }
                cache.line_hash = line_hash;
                cache.is_valid = true;
                cache.needs_update = true; // Will be updated when new tokens arrive
                DBG_TEDITOR(DebugModule::CACHE, "LineCache", "Created default token for line %d", line_number);
            }
        }
    }

    return FilterVisibleTokens(cache.tokens);
}

std::vector<SyntaxToken> TextEditor::FilterVisibleTokens(const std::vector<SyntaxToken>& tokens) {
    std::vector<SyntaxToken> visible_tokens;
    visible_tokens.reserve(tokens.size());

    int visible_count = 0;
    for (const auto& token : tokens) {
        float token_start = token.column;
        float token_end = token.column + token.length;

        if (token_end >= visible_column_start_ &&
            token_start <= visible_column_start_ + visible_column_width_) {
            visible_tokens.push_back(token);
            visible_count++;
        }
    }

    //DBG_TEDITOR(DebugModule::RENDER, "FilterTokens", "Filtered %d/%zu tokens (visible col %f-%f)",visible_count, tokens.size(), visible_column_start_,visible_column_start_ + visible_column_width_);

    return visible_tokens;
}

void TextEditor::CalculateVisibleArea() {
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (!g) return;

    float window_height = ImGui::GetWindowHeight();
    float line_height = ImGui::GetTextLineHeightWithSpacing();

    int old_line_count = visible_line_count_;
    int old_line_start = visible_line_start_;

    visible_line_count_ = static_cast<int>(window_height / line_height) + 2;

    float scroll_y = ImGui::GetScrollY();
    visible_line_start_ = std::max(0, static_cast<int>(scroll_y / line_height) - 1);
    visible_line_start_ = std::min(visible_line_start_, static_cast<int>(lines_.size()) - 1);

    float scroll_x = ImGui::GetScrollX();
    visible_column_start_ = scroll_x / ImGui::GetTextLineHeightWithSpacing();
    visible_column_width_ = ImGui::GetContentRegionAvail().x / ImGui::GetTextLineHeightWithSpacing();

    if (old_line_start != visible_line_start_ || old_line_count != visible_line_count_) {
        DBG_TEDITOR(DebugModule::RENDER, "VisibleArea",
            "Updated: lines %d-%d (count=%d), cols %.1f-%.1f",
            visible_line_start_, visible_line_start_ + visible_line_count_,
            visible_line_count_, visible_column_start_,
            visible_column_start_ + visible_column_width_);
    }
}

void TextEditor::UpdateContentFromLines(int start_line, int end_line)
{
    if (end_line < 0) {
        end_line = lines_.size() - 1;
        DBG_TEDITOR(DebugModule::EDIT, "UpdateContent", "Updating all lines (0-%d)", end_line);
    }
    else {
        DBG_TEDITOR(DebugModule::EDIT, "UpdateContent", "Updating lines %d-%d", start_line, end_line);
    }

    content_dirty_ = true;
    uint64_t old_version = content_version_.load();
    ++content_version_;

    DBG_TEDITOR(DebugModule::EDIT, "ContentVersion", "Version %llu -> %llu",
        static_cast<unsigned long long>(old_version),
        static_cast<unsigned long long>(content_version_.load()));

    // keep cache vectors in sync with buffer size
    if (line_token_cache_.size() != lines_.size()) {
        DBG_TEDITOR(DebugModule::CACHE, "Resize", "Resizing line cache from %zu to %zu",
            line_token_cache_.size(), lines_.size());
        line_token_cache_.resize(lines_.size());
    }
    {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        if (tokens_by_line_.size() != lines_.size()) {
            DBG_TEDITOR(DebugModule::CACHE, "Resize", "Resizing tokens array from %zu to %zu",
                tokens_by_line_.size(), lines_.size());
            tokens_by_line_.resize(lines_.size());
        }
    }

    // mark lines dirty
    int marked_count = 0;
    if (start_line >= 0 && end_line >= 0) {
        for (int i = start_line; i <= end_line &&
            i < static_cast<int>(line_token_cache_.size());
            ++i) {
            line_token_cache_[i].needs_update = true;
            marked_count++;
        }
    }
    else {
        for (auto& c : line_token_cache_) {
            c.needs_update = true;
            marked_count++;
        }
    }

    DBG_TEDITOR(DebugModule::CACHE, "MarkDirty", "Marked %d lines as needing update", marked_count);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_edit_time_);

    if (elapsed >= HIGHLIGHT_DEBOUNCE) {
        if (highlight_pending_) {
            highlight_dirty_ = true;
            DBG_TEDITOR(DebugModule::HIGHLIGHT, "Debounce",
                "Highlight pending, marked dirty (elapsed %lld ms)", elapsed.count());
        }
        else {
            DBG_TEDITOR(DebugModule::HIGHLIGHT, "Debounce",
                "Triggering highlight immediately (elapsed %lld ms)", elapsed.count());
            UpdateHighlightingAsync();
        }
    }
    else {
        DBG_TEDITOR(DebugModule::HIGHLIGHT, "Debounce",
            "Deferring highlight (elapsed %lld ms < %lld ms)",
            elapsed.count(), HIGHLIGHT_DEBOUNCE.count());
    }
    last_edit_time_ = now;
}

void TextEditor::SaveUndo()
{
    size_t old_size = undo_stack_.size();
    undo_stack_.push_back({ GetContent(), cursor_ });

    if (undo_stack_.size() > MAX_UNDO_STACK) {
        size_t removed = undo_stack_.size() - MAX_UNDO_STACK;
        undo_stack_.erase(undo_stack_.begin(),
            undo_stack_.begin() + removed);
        DBG_TEDITOR(DebugModule::UNDO, "Trim", "Removed %zu old undo states", removed);
    }

    size_t redo_cleared = redo_stack_.size();
    redo_stack_.clear();

    DBG_TEDITOR(DebugModule::UNDO, "Save", "Saved undo state #%zu (cleared %zu redo states)",
        undo_stack_.size(), redo_cleared);
}

void TextEditor::Undo()
{
    if (undo_stack_.empty()) {
        DBG_TEDITOR(DebugModule::UNDO, "Undo", "No undo states available");
        return;
    }

    DBG_TEDITOR(DebugModule::UNDO, "Undo", "Performing undo (stack size: %zu -> %zu)",
        undo_stack_.size(), undo_stack_.size() - 1);

    redo_stack_.push_back({ GetContent(), cursor_ });

    auto state = undo_stack_.back();
    undo_stack_.pop_back();

    SetContent(state.content);
    cursor_ = state.cursor;
    scrollToCursor_ = true;

    DBG_TEDITOR(DebugModule::UNDO, "Undo", "Restored state, cursor at (%d, %d)",
        cursor_.line, cursor_.column);
}

void TextEditor::Redo()
{
    if (redo_stack_.empty()) {
        DBG_TEDITOR(DebugModule::UNDO, "Redo", "No redo states available");
        return;
    }

    DBG_TEDITOR(DebugModule::UNDO, "Redo", "Performing redo (stack size: %zu -> %zu)",
        redo_stack_.size(), redo_stack_.size() - 1);

    undo_stack_.push_back({ GetContent(), cursor_ });

    auto state = redo_stack_.back();
    redo_stack_.pop_back();

    SetContent(state.content);
    cursor_ = state.cursor;
    scrollToCursor_ = true;

    DBG_TEDITOR(DebugModule::UNDO, "Redo", "Restored state, cursor at (%d, %d)",
        cursor_.line, cursor_.column);
}

void TextEditor::InsertChar(char c)
{
    DBG_TEDITOR(DebugModule::EDIT, "InsertChar", "Inserting '%c' (0x%02X) at (%d, %d)",
        isprint(c) ? c : '?', (unsigned char)c, cursor_.line, cursor_.column);

    if (has_selection_) {
        DBG_TEDITOR(DebugModule::SELECTION, "Clear", "Clearing selection before insert");
        DeleteSelectedText();
        typing_session_ = false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_type_time_);

    if (!typing_session_ || elapsed > TYPING_DEBOUNCE) {
        DBG_TEDITOR(DebugModule::UNDO, "Session", "Starting new typing session (elapsed %lld ms)",
            elapsed.count());
        SaveUndo();
        typing_session_ = true;
    }
    last_type_time_ = now;

    lines_[cursor_.line].insert(cursor_.column, 1, c);
    cursor_.column++;

    DBG_TEDITOR(DebugModule::CURSOR, "Move", "Cursor moved to (%d, %d)", cursor_.line, cursor_.column);

    UpdateContentFromLines(cursor_.line, cursor_.line);
}

void TextEditor::InsertNewLine()
{
    DBG_TEDITOR(DebugModule::EDIT, "NewLine", "Inserting newline at (%d, %d)", cursor_.line, cursor_.column);

    if (has_selection_) {
        DBG_TEDITOR(DebugModule::SELECTION, "Clear", "Clearing selection before newline");
        DeleteSelectedText();
    }

    SaveUndo();
    typing_session_ = false;

    auto& line = lines_[cursor_.line];
    std::string new_line = SafeSubstr(line, cursor_.column);
    line = SafeSubstr(line, 0, cursor_.column);

    DBG_TEDITOR(DebugModule::EDIT, "Split", "Split line %d: '%s' | '%s'",
        cursor_.line, line.c_str(), new_line.c_str());

    lines_.insert(lines_.begin() + cursor_.line + 1, new_line);
    InsertLineCaches(cursor_.line + 1);

    cursor_.line++;
    cursor_.column = 0;
    scrollToCursor_ = true;

    DBG_TEDITOR(DebugModule::CURSOR, "Move", "Cursor moved to (%d, %d)", cursor_.line, cursor_.column);

    UpdateContentFromLines(cursor_.line - 1, lines_.size() - 1);
}

void TextEditor::DeleteChar()
{
    if (has_selection_) {
        DBG_TEDITOR(DebugModule::EDIT, "Delete", "Deleting selection");
        DeleteSelectedText();
        deleting_session_ = false;
        return;
    }

    if (cursor_.column == 0 && cursor_.line == 0) {
        DBG_TEDITOR(DebugModule::EDIT, "Delete", "At beginning of document, nothing to delete");
        return;
    }

    DBG_TEDITOR(DebugModule::EDIT, "Backspace", "Deleting char at (%d, %d)", cursor_.line, cursor_.column);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_delete_time_);

    if (!deleting_session_ || elapsed > TYPING_DEBOUNCE) {
        DBG_TEDITOR(DebugModule::UNDO, "Session", "Starting new delete session (elapsed %lld ms)",
            elapsed.count());
        SaveUndo();
        deleting_session_ = true;
    }
    last_delete_time_ = now;

    if (cursor_.column == 0) {
        DBG_TEDITOR(DebugModule::EDIT, "MergeLines", "Merging line %d with line %d",
            cursor_.line, cursor_.line - 1);
        cursor_.line--;
        cursor_.column = lines_[cursor_.line].length();
        lines_[cursor_.line] += lines_[cursor_.line + 1];
        lines_.erase(lines_.begin() + cursor_.line + 1);
        EraseLineCaches(cursor_.line + 1);

        UpdateContentFromLines(cursor_.line, lines_.size() - 1);
    }
    else {
        char deleted_char = lines_[cursor_.line][cursor_.column - 1];
        DBG_TEDITOR(DebugModule::EDIT, "DeleteChar", "Deleting '%c' (0x%02X)",
            isprint(deleted_char) ? deleted_char : '?', (unsigned char)deleted_char);

        lines_[cursor_.line].erase(cursor_.column - 1, 1);
        cursor_.column--;
        UpdateContentFromLines(cursor_.line, cursor_.line);
    }

    DBG_TEDITOR(DebugModule::CURSOR, "Move", "Cursor at (%d, %d)", cursor_.line, cursor_.column);
}

void TextEditor::MoveCursorLeft()
{
    CursorPosition old_pos = cursor_;

    if (cursor_.column > 0) {
        cursor_.column--;
    }
    else if (cursor_.line > 0) {
        cursor_.line--;
        cursor_.column = lines_[cursor_.line].length();
    }

    DBG_TEDITOR(DebugModule::CURSOR, "Left", "Moved from (%d, %d) to (%d, %d)",
        old_pos.line, old_pos.column, cursor_.line, cursor_.column);
}

void TextEditor::MoveCursorRight()
{
    CursorPosition old_pos = cursor_;

    if (cursor_.column < lines_[cursor_.line].length()) {
        cursor_.column++;
    }
    else if (cursor_.line < lines_.size() - 1) {
        cursor_.line++;
        cursor_.column = 0;
    }

    DBG_TEDITOR(DebugModule::CURSOR, "Right", "Moved from (%d, %d) to (%d, %d)",
        old_pos.line, old_pos.column, cursor_.line, cursor_.column);
}

void TextEditor::MoveCursorUp()
{
    CursorPosition old_pos = cursor_;

    if (cursor_.line > 0) {
        cursor_.line--;
        cursor_.column = std::min(cursor_.column,
            static_cast<int>(lines_[cursor_.line].length()));
    }

    DBG_TEDITOR(DebugModule::CURSOR, "Up", "Moved from (%d, %d) to (%d, %d)",
        old_pos.line, old_pos.column, cursor_.line, cursor_.column);
}

void TextEditor::MoveCursorDown()
{
    CursorPosition old_pos = cursor_;

    if (cursor_.line < lines_.size() - 1) {
        cursor_.line++;
        cursor_.column = std::min(cursor_.column,
            static_cast<int>(lines_[cursor_.line].length()));
    }

    DBG_TEDITOR(DebugModule::CURSOR, "Down", "Moved from (%d, %d) to (%d, %d)",
        old_pos.line, old_pos.column, cursor_.line, cursor_.column);
}

std::string TextEditor::GetSelectedText() {
    if (!has_selection_) {
        DBG_TEDITOR(DebugModule::SELECTION, "GetText", "No selection active");
        return "";
    }

    CursorPosition start = std::min(cursor_, selection_start_);
    CursorPosition end = std::max(cursor_, selection_start_);

    DBG_TEDITOR(DebugModule::SELECTION, "GetText", "Getting text from (%d, %d) to (%d, %d)",
        start.line, start.column, end.line, end.column);

    if (start.line == end.line) {
        return lines_[start.line].substr(start.column, end.column - start.column);
    }

    std::string result;
    result += lines_[start.line].substr(start.column);
    result += '\n';

    for (int i = start.line + 1; i < end.line; ++i) {
        result += lines_[i];
        result += '\n';
    }

    result += lines_[end.line].substr(0, end.column);

    DBG_TEDITOR(DebugModule::SELECTION, "GetText", "Selected text: %zu bytes", result.size());
    return result;
}

void TextEditor::DeleteSelectedText() {
    if (!has_selection_) {
        DBG_TEDITOR(DebugModule::SELECTION, "Delete", "No selection to delete");
        return;
    }

    SaveUndo();

    CursorPosition start = std::min(cursor_, selection_start_);
    CursorPosition end = std::max(cursor_, selection_start_);
    size_t removed = end.line - start.line;

    DBG_TEDITOR(DebugModule::SELECTION, "Delete", "Deleting from (%d, %d) to (%d, %d)",
        start.line, start.column, end.line, end.column);

    if (start.line == end.line) {
        lines_[start.line].erase(start.column, end.column - start.column);
        EraseLineCaches(start.line + 1, removed);
        UpdateContentFromLines(start.line, start.line);
    }
    else {
        lines_[start.line] = lines_[start.line].substr(0, start.column)
            + lines_[end.line].substr(end.column);
        lines_.erase(lines_.begin() + start.line + 1,
            lines_.begin() + end.line + 1);

        EraseLineCaches(start.line + 1, removed);
        UpdateContentFromLines(start.line, lines_.size() - 1);

        DBG_TEDITOR(DebugModule::SELECTION, "Delete", "Removed %zu lines", removed);
    }

    cursor_ = start;
    has_selection_ = false;

    DBG_TEDITOR(DebugModule::CURSOR, "Reset", "Cursor reset to (%d, %d)", cursor_.line, cursor_.column);
}

void TextEditor::PasteText(const std::string& text) {
    DBG_TEDITOR(DebugModule::CLIPBOARD, "Paste", "Pasting %zu bytes at (%d, %d)",
        text.size(), cursor_.line, cursor_.column);

    if (has_selection_) {
        DBG_TEDITOR(DebugModule::SELECTION, "Clear", "Clearing selection before paste");
        DeleteSelectedText();
    }

    SaveUndo();

    // 1) Split the incoming text into lines
    std::vector<std::string> newLines;
    std::istringstream iss(text);
    std::string row;
    while (std::getline(iss, row)) {
        newLines.push_back(row);
    }
    if (newLines.empty()) newLines.push_back("");

    DBG_TEDITOR(DebugModule::CLIPBOARD, "Parse", "Parsed %zu lines from clipboard", newLines.size());

    // 2) Capture prefix/suffix of the current line
    auto& curLine = lines_[cursor_.line];
    std::string prefix = curLine.substr(0, cursor_.column);
    std::string suffix = curLine.substr(cursor_.column);

    // 3) Replace the current line with prefix + first pasted line
    lines_[cursor_.line] = prefix + newLines[0];

    // 4) Insert any middle lines
    for (size_t i = 1; i < newLines.size(); ++i) {
        lines_.insert(
            lines_.begin() + cursor_.line + i,
            newLines[i]
        );
        InsertLineCaches(cursor_.line + i);
    }

    // 5) Append the saved suffix onto the very last pasted line
    size_t lastLine = cursor_.line + newLines.size() - 1;
    lines_[lastLine] += suffix;

    // 6) Move the cursor to the end of the pasted text
    cursor_.line = static_cast<int>(lastLine);
    cursor_.column = static_cast<int>(newLines.back().size());

    // 7) Mark that we need to scroll so the cursor is visible
    scrollToCursor_ = true;

    // 8) Update from start line to end
    UpdateContentFromLines(cursor_.line, lines_.size() - 1);

    DBG_TEDITOR(DebugModule::CURSOR, "Move", "Cursor at (%d, %d) after paste",
        cursor_.line, cursor_.column);
}

void TextEditor::InsertTextAtCursor(const std::string& text) {
    DBG_TEDITOR(DebugModule::EDIT, "InsertText", "Inserting %zu bytes at cursor", text.size());

    if (has_selection_) {
        DeleteSelectedText();
    }

    SaveUndo();

    int start_line = cursor_.line;
    int chars_inserted = 0;
    int newlines_inserted = 0;

    for (char c : text) {
        if (c == '\n') {
            InsertNewLine();
            newlines_inserted++;
        }
        else {
            lines_[cursor_.line].insert(cursor_.column++, 1, c);
            chars_inserted++;
        }
    }

    DBG_TEDITOR(DebugModule::EDIT, "InsertText",
        "Inserted %d chars and %d newlines", chars_inserted, newlines_inserted);

    UpdateContentFromLines(start_line, cursor_.line);
}

void TextEditor::DrawFindReplacePanel() {
    //DBG_TEDITOR(DebugModule::RENDER, "FindPanel", "Drawing find/replace panel");

    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.95f); // semi-transparent
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Pos.x + 20, ImGui::GetMainViewport()->Pos.y + 20), ImGuiCond_FirstUseEver);

    ImGui::Begin("Find / Replace", &show_find_panel_, ImGuiWindowFlags_AlwaysAutoResize);

    static bool find_case_sensitive = false;
    static char find_buf[512] = "";
    static char replace_buf[512] = "";
    strncpy(find_buf, find_query_.c_str(), sizeof(find_buf));
    strncpy(replace_buf, replace_text_.c_str(), sizeof(replace_buf));

    ImGui::InputText("Find", find_buf, sizeof(find_buf));
    ImGui::SameLine();
    ImGui::Checkbox("Regex", &find_use_regex_);
    ImGui::SameLine();
    ImGui::Checkbox("Case Sensitive", &find_case_sensitive_);
    ImGui::InputText("Replace", replace_buf, sizeof(replace_buf));

    find_query_ = find_buf;
    replace_text_ = replace_buf;

    if (ImGui::Button("Find All")) {
        DBG_TEDITOR(DebugModule::SEARCH, "FindAll", "Searching for: %s", find_query_.c_str());

        find_results_.clear();
        for (int i = 0; i < lines_.size(); ++i) {
            int start = 0, len = 0;
            if (MatchFind(lines_[i], start, len)) {
                find_results_.emplace_back(CursorPosition{ i, start });
            }
        }
        current_find_index_ = 0;

        DBG_TEDITOR(DebugModule::SEARCH, "FindAll", "Found %zu matches", find_results_.size());

        if (!find_results_.empty()) {
            cursor_ = find_results_[0];
            scrollToCursor_ = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Replace All")) {
        DBG_TEDITOR(DebugModule::SEARCH, "ReplaceAll", "Replacing '%s' with '%s'",
            find_query_.c_str(), replace_text_.c_str());

        SaveUndo();
        int total_replacements = 0;

        for (int i = 0; i < lines_.size(); ++i) {
            size_t search_pos = 0;
            int start = 0, len = 0;
            int line_replacements = 0;

            while (MatchFind(lines_[i].substr(search_pos), start, len)) {
                lines_[i].replace(search_pos + start, len, replace_text_);
                search_pos += start + replace_text_.length();
                line_replacements++;
                total_replacements++;
            }

            if (line_replacements > 0) {
                DBG_TEDITOR(DebugModule::SEARCH, "ReplaceLine",
                    "Line %d: %d replacements", i, line_replacements);
            }
        }

        DBG_TEDITOR(DebugModule::SEARCH, "ReplaceAll", "Total replacements: %d", total_replacements);
        UpdateContentFromLines();
    }

    if (!find_results_.empty()) {
        if (ImGui::Button("Previous")) {
            if (--current_find_index_ < 0)
                current_find_index_ = (int)find_results_.size() - 1;
            cursor_ = find_results_[current_find_index_];
            scrollToCursor_ = true;

            DBG_TEDITOR(DebugModule::SEARCH, "Navigate", "Previous match: %d/%zu at (%d, %d)",
                current_find_index_ + 1, find_results_.size(),
                cursor_.line, cursor_.column);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next")) {
            if (++current_find_index_ >= (int)find_results_.size())
                current_find_index_ = 0;
            cursor_ = find_results_[current_find_index_];
            scrollToCursor_ = true;

            DBG_TEDITOR(DebugModule::SEARCH, "Navigate", "Next match: %d/%zu at (%d, %d)",
                current_find_index_ + 1, find_results_.size(),
                cursor_.line, cursor_.column);
        }
    }

    ImGui::Text("Matches: %d", (int)find_results_.size());
    ImGui::End();
}

void TextEditor::DrawMinimap()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2      canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2      canvas_size = ImGui::GetContentRegionAvail();
    float       minimap_w = canvas_size.x;
    float       minimap_h = canvas_size.y;

    // vertical scale: pixel-per-line, clamped
    const float kMaxLineH = 7.5f;
    float scale = minimap_h / std::max(1, (int)lines_.size());
    scale = std::min(scale, kMaxLineH);

    ImFont* font = ImGui::GetFont();
    float   font_scale = 0.35f;
    float   font_size = font->FontSize * font_scale;

    // 1) Find the widest line in pixels
    float max_line_w = 0.0f;
    for (auto& line : lines_) {
        float w = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, line.c_str()).x;
        max_line_w = std::max(max_line_w, w);
    }

    // 2) Compute horizontal scale so max_line_w * hScale == minimap_w
    float hScale = (max_line_w > 0.0f)
        ? (minimap_w / max_line_w)
        : 1.0f;

    // reserve space & handle clicks (unchanged)
    ImGui::InvisibleButton("##Minimap", ImVec2(minimap_w, minimap_h));
    if (ImGui::IsItemActive()) {
        ImVec2 mouse = ImGui::GetMousePos();
        int lineHit = std::clamp(int((mouse.y - canvas_pos.y) / scale),
            0, (int)lines_.size() - 1);
        float lineH = ImGui::GetTextLineHeightWithSpacing();
        scrollToLineY_ = lineHit * lineH
            - (visible_line_count_ * 0.5f) * lineH;
    }

    // clip to minimap rect
    draw_list->PushClipRect(
        canvas_pos,
        ImVec2(canvas_pos.x + minimap_w, canvas_pos.y + minimap_h),
        true
    );

    // now draw each line
    for (int i = 0; i < (int)lines_.size(); ++i) {
        float y0 = canvas_pos.y + i * scale;

        // background
        ImU32 bg = IM_COL32(100, 100, 100, 100);
        if (i >= visible_line_start_ &&
            i < visible_line_start_ + visible_line_count_)
            bg = IM_COL32(180, 180, 255, 150);
        if (std::any_of(find_results_.begin(), find_results_.end(),
            [i](auto& m) { return m.line == i; }))
            bg = IM_COL32(255, 255, 100, 180);

        draw_list->AddRectFilled(
            ImVec2(canvas_pos.x, y0),
            ImVec2(canvas_pos.x + minimap_w, y0 + scale),
            bg
        );

        // gather tokens
        std::vector<SyntaxToken> toks;
        {
            std::lock_guard<std::mutex> lk(tokens_mutex_);
            if (i < tokens_by_line_.size())
                toks = tokens_by_line_[i];
        }

        // un-scaled x offset (pixels)
        float x_unscaled = 0.0f;

        // draw plain+token+trailing in sequence
        int col = 0;
        for (auto& t : toks) {
            // plain text before this token
            if (t.column > col) {
                std::string txt = SafeSubstr(lines_[i], col, t.column - col);
                ImU32 colTxt = IM_COL32(220, 220, 220, 160);

                // compute display position
                float x_disp = canvas_pos.x + x_unscaled * hScale;
                draw_list->AddText(
                    font,
                    font_size * hScale,
                    ImVec2(x_disp, y0),
                    colTxt,
                    txt.c_str()
                );
                // advance unscaled offset
                x_unscaled += font->CalcTextSizeA(
                    font_size, FLT_MAX, 0.0f, txt.c_str()).x;
            }

            // the token itself
            std::string tokTxt = SafeSubstr(lines_[i], t.column, t.length);
            ImU32 colTok = ImGui::ColorConvertFloat4ToU32(t.color);
            float  x_disp = canvas_pos.x + x_unscaled * hScale;
            draw_list->AddText(
                font,
                font_size * hScale,
                ImVec2(x_disp, y0),
                colTok,
                tokTxt.c_str()
            );
            x_unscaled += font->CalcTextSizeA(
                font_size, FLT_MAX, 0.0f, tokTxt.c_str()).x;

            col = t.column + t.length;
        }

        // trailing text
        if (col < (int)lines_[i].size()) {
            std::string rest = SafeSubstr(lines_[i], col);
            ImU32 colTxt = IM_COL32(220, 220, 220, 160);
            float x_disp = canvas_pos.x + x_unscaled * hScale;
            draw_list->AddText(
                font,
                font_size * hScale,
                ImVec2(x_disp, y0),
                colTxt,
                rest.c_str()
            );
        }
    }

    draw_list->PopClipRect();
}




void TextEditor::Draw() {
    ProcessPendingHighlights();
    ProcessPendingSemantics();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float totalW = avail.x;
    float minimapW = totalW * 0.10f;    // always 10%
    float editorW = totalW - minimapW; // the other 90%

    ImGui::SetWindowFontScale(font_scale_);
    ImVec2 gutterSize = ImGui::CalcTextSize("9999 | ");
    float gutterWidth = gutterSize.x;
    if (show_find_panel_)
        DrawFindReplacePanel();
    ImGui::BeginChild("TextEditor", ImVec2(editorW, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    CalculateVisibleArea();
    if (scrollToLineY_) {
        ImGui::SetScrollY(std::max(0.0f, *scrollToLineY_));
        scrollToLineY_.reset();
    }

    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            show_find_panel_ = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) {
            show_find_panel_ = true;
        }
    }


    if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
        font_scale_ += ImGui::GetIO().MouseWheel * 0.1f;
        font_scale_ = std::clamp(font_scale_, 0.5f, 3.0f); // clamp to reasonable range
    }

    // Handle keyboard input
    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive()) {
        // Ctrl+C/V/X/Z/Y
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                if (has_selection_) {
                    ImGui::SetClipboardText(GetSelectedText().c_str());
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V)) {
                if (const char* cb = ImGui::GetClipboardText())
                    PasteText(cb);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                if (has_selection_) {
                    ImGui::SetClipboardText(GetSelectedText().c_str());
                    DeleteSelectedText();
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
                Undo();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
                Redo();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                selection_start_ = { 0, 0 };
                cursor_ = { static_cast<int>(lines_.size() - 1), static_cast<int>(lines_.back().length()) };
                has_selection_ = true;
            }
        }

        // Navigation
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            MoveCursorLeft();
            if (!io.KeyShift) {
                ClearSelection();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            MoveCursorRight();
            if (!io.KeyShift) {
                ClearSelection();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            MoveCursorUp();
            if (!io.KeyShift) {
                ClearSelection();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            MoveCursorDown();
            if (!io.KeyShift) {
                ClearSelection();
            }
        }

        // Home/End
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            cursor_.column = 0;
            if (!io.KeyShift) {
                ClearSelection();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_End)) {
            if (io.KeyShift && !has_selection_) {
                SetSelection(cursor_);
            }
            cursor_.column = lines_[cursor_.line].length();
            if (!io.KeyShift) {
                ClearSelection();
            }
        }

        // Editing
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            // If you want a single undo‐step for the whole tab:
            SaveUndo();
            InsertTextAtCursor("    ");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            InsertNewLine();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            DeleteChar();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (has_selection_) {
                DeleteSelectedText();
            }
            else if (cursor_.column < lines_[cursor_.line].length()) {
                SaveUndo();
                lines_[cursor_.line].erase(cursor_.column, 1);
                UpdateContentFromLines(cursor_.line, cursor_.line);
            }
            else if (cursor_.line < lines_.size() - 1) {
                SaveUndo();
                lines_[cursor_.line] += lines_[cursor_.line + 1];
                lines_.erase(lines_.begin() + cursor_.line + 1);
                UpdateContentFromLines(cursor_.line, lines_.size() - 1);
            }
        }

        // Text input
        if (io.InputQueueCharacters.Size > 0) {
            for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                auto c = io.InputQueueCharacters[n];
                if (c != 0 && c != '\n' && c != '\r') {
                    InsertChar(static_cast<char>(c));
                }
            }
            io.InputQueueCharacters.resize(0);
        }
    }

    // Handle mouse input
    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            // 1) Update click count based on timing
            double now = ImGui::GetTime();
            if (now - lastClickTime_ < ImGui::GetIO().MouseDoubleClickTime) {
                clickCount_ = std::min(clickCount_ + 1, 3);
            }
            else {
                clickCount_ = 1;
            }
            lastClickTime_ = now;

            // 2) Figure out which line/column was clicked
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 window_pos = ImGui::GetWindowPos();
            float  line_h = ImGui::GetTextLineHeightWithSpacing();
            int    clickedLine = static_cast<int>((mouse_pos.y - window_pos.y + ImGui::GetScrollY()) / line_h);
            clickedLine = std::clamp(clickedLine, 0, (int)lines_.size() - 1);

            float x_offset = mouse_pos.x - window_pos.x - gutterWidth;
            int   clickedCol = 0;
            {
                const std::string& line = lines_[clickedLine];
                float accum = 0;
                for (int i = 0; i < line.size(); ++i) {
                    float w = ImGui::CalcTextSize(SafeSubstr(line, i, 1).c_str()).x;
                    if (accum + w * 0.5f > x_offset + ImGui::GetScrollX())
                        break;
                    accum += w;
                    clickedCol++;
                }
            }

            // 3) Dispatch based on clickCount_
            if (clickCount_ == 2) {
                // double-click → select word
                cursor_ = { clickedLine, clickedCol };
                SelectWordAt(cursor_);
            }
            else if (clickCount_ >= 3) {
                // triple-click → select entire line
                SelectLineAt(clickedLine);
            }
            else {
                // single-click → move cursor & start/cancel selection
                cursor_ = { clickedLine, clickedCol };
                if (ImGui::GetIO().KeyShift) {
                    if (!has_selection_) SetSelection(cursor_);
                }
                else {
                    ClearSelection();
                }
                is_selecting_with_mouse_ = true;
            }
        }

        if (ImGui::IsMouseDragging(0) && is_selecting_with_mouse_) {
            if (!has_selection_) {
                SetSelection(cursor_);
            }

            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 window_pos = ImGui::GetWindowPos();
            float line_height = ImGui::GetTextLineHeightWithSpacing();

            // Corrected: subtract scroll Y
            int clicked_line = static_cast<int>((mouse_pos.y - window_pos.y + ImGui::GetScrollY()) / line_height);
            clicked_line = std::clamp(clicked_line, 0, static_cast<int>(lines_.size()) - 1);

            float x_offset = mouse_pos.x - window_pos.x - gutterWidth;
            int column = 0;
            if (clicked_line < lines_.size()) {
                const std::string& line = lines_[clicked_line];
                float text_width = 0;
                for (int i = 0; i < line.length(); ++i) {
                    float char_width = ImGui::CalcTextSize(SafeSubstr(line, i, 1).c_str()).x;
                    if (text_width + char_width * 0.5f > x_offset + ImGui::GetScrollX()) {
                        break;
                    }
                    text_width += char_width;
                    column++;
                }
            }

            cursor_ = { clicked_line, column };
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 window_pos = ImGui::GetWindowPos();
            float line_h = ImGui::GetTextLineHeightWithSpacing();
            int clicked_line = static_cast<int>((mouse_pos.y - window_pos.y + ImGui::GetScrollY()) / line_h);
            clicked_line = std::clamp(clicked_line, 0, (int)lines_.size() - 1);

            float x_offset = mouse_pos.x - window_pos.x - gutterWidth;
            int clicked_col = 0;
            {
                const std::string& line = lines_[clicked_line];
                float accum = 0;
                for (int i = 0; i < line.size(); ++i) {
                    float w = ImGui::CalcTextSize(SafeSubstr(line, i, 1).c_str()).x;
                    if (accum + w * 0.5f > x_offset + ImGui::GetScrollX())
                        break;
                    accum += w;
                    clicked_col++;
                }
            }

            // If no selection, move cursor to click location
            if (!has_selection_) {
                cursor_ = { clicked_line, clicked_col };
                ClearSelection();
            }

            // Open context menu popup
            ImGui::OpenPopup("TextEditorContextMenu");
        }

        if (ImGui::IsMouseReleased(0)) {
            is_selecting_with_mouse_ = false;
        }
    }

    if (ImGui::BeginPopup("TextEditorContextMenu")) {
        if (has_selection_) {
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                ImGui::SetClipboardText(GetSelectedText().c_str());
            }

            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                if (const char* cb = ImGui::GetClipboardText())
                    PasteText(cb);
            }

            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                ImGui::SetClipboardText(GetSelectedText().c_str());
                DeleteSelectedText();
            }
        }
        else {
            if (ImGui::MenuItem("Copy Line")) {
                ImGui::SetClipboardText(lines_[cursor_.line].c_str());
            }

            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                if (const char* cb = ImGui::GetClipboardText())
                    PasteText(cb);
            }

            if (ImGui::MenuItem("Cut Line")) {
                SaveUndo();
                ImGui::SetClipboardText(lines_[cursor_.line].c_str());
                lines_.erase(lines_.begin() + cursor_.line);
                if (lines_.empty()) lines_.push_back("");
                cursor_.line = std::min(cursor_.line, (int)lines_.size() - 1);
                cursor_.column = std::min(cursor_.column, (int)lines_[cursor_.line].size());
                UpdateContentFromLines();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undo_stack_.empty())) {
                Undo();
            }

            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redo_stack_.empty())) {
                Redo();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                selection_start_ = { 0, 0 };
                cursor_ = { static_cast<int>(lines_.size() - 1), static_cast<int>(lines_.back().length()) };
                has_selection_ = true;
            }
        }

        ImGui::EndPopup();
    }

    if (scrollToCursor_) {
        // Vertical scroll only if cursor is off-screen
        if (cursor_.line < visible_line_start_ ||
            cursor_.line >= visible_line_start_ + visible_line_count_)
        {
            float lineH = ImGui::GetTextLineHeightWithSpacing();
            // center cursor line in view
            float targetY = cursor_.line * lineH - (visible_line_count_ / 2) * lineH;
            ImGui::SetScrollY(std::max(0.0f, targetY));
        }

        // Horizontal scroll only if cursor column is off-screen
        float scrollX = ImGui::GetScrollX();
        float availW = ImGui::GetContentRegionAvail().x;
        // measure the width of all text up to the cursor
        std::string  line = lines_[cursor_.line];
        std::string  before = SafeSubstr(line, 0, cursor_.column);
        float cursorPx = ImGui::CalcTextSize(before.c_str()).x;

        // if the cursor is left of scroll or right of visible area, recenter it
        if (cursorPx < scrollX || cursorPx > scrollX + availW) {
            float targetX = cursorPx - (availW * 0.5f);
            ImGui::SetScrollX(std::max(0.0f, targetX));
        }
        scrollToCursor_ = false;
    }

    ImVec2 window_pos = ImGui::GetWindowPos();
    float window_width = ImGui::GetWindowWidth();

    int end_line = std::min(visible_line_start_ + visible_line_count_,
        static_cast<int>(lines_.size()));

    if (visible_line_start_ > 0) {
        float skip_height = visible_line_start_ * ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + skip_height);
    }

    std::map<std::pair<int, int>, std::string> local_sem_kind;
    {
        std::lock_guard<std::mutex> lock(semantic_mutex_);
        local_sem_kind = sem_kind_;
    }

    for (int lineNo = visible_line_start_; lineNo < end_line; ++lineNo) {
        char buf[32];
        sprintf(buf, "%4d | ", lineNo + 1);
        ImGui::TextUnformatted(buf);
        ImGui::SameLine(0, 0);
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        ImVec2 text_start = ImGui::GetCursorScreenPos();

        if (!find_results_.empty()) {
            // Highlight matched lines and matches
            for (const auto& match : find_results_) {
                if (match.line == lineNo) {
                    // Highlight the entire line (dim background)
                    ImVec2 highlight_start = ImVec2(window_pos.x, text_start.y);
                    ImVec2 highlight_end = ImVec2(window_pos.x + window_width, text_start.y + line_height);
                    ImGui::GetWindowDrawList()->AddRectFilled(highlight_start, highlight_end, IM_COL32(60, 80, 20, 60));

                    // Highlight the matched substring (stronger highlight)
                    int match_col = match.column;
                    std::string match_text = SafeSubstr(lines_[lineNo], match_col, find_query_.length());

                    ImVec2 match_start = text_start;
                    match_start.x += ImGui::CalcTextSize(SafeSubstr(lines_[lineNo], 0, match_col).c_str()).x;

                    ImVec2 match_end = match_start;
                    match_end.x += ImGui::CalcTextSize(match_text.c_str()).x;
                    match_end.y += line_height;

                    ImGui::GetWindowDrawList()->AddRectFilled(match_start, match_end, IM_COL32(200, 200, 0, 100));
                }
            }
        }

        const std::string& line = lines_[lineNo];

        bool is_cursor_line = (cursor_.line == lineNo);
        if (is_cursor_line) {
            ImVec2 highlight_start = ImVec2(window_pos.x, text_start.y);
            ImVec2 highlight_end = ImVec2(window_pos.x + window_width, text_start.y + line_height);
            ImGui::GetWindowDrawList()->AddRectFilled(highlight_start, highlight_end,
                IM_COL32(60, 60, 120, 60));
        }

        static float blink_timer = 0.0f;
        static bool blink_on = true;

        blink_timer += io.DeltaTime;
        if (blink_timer >= 15) {
            blink_timer = 0.0f;
            blink_on = !blink_on;
        }

        if (is_cursor_line && blink_on && ImGui::IsWindowFocused()) {
            float x = text_start.x + ImGui::CalcTextSize(SafeSubstr(line, 0, cursor_.column).c_str()).x;
            float y = text_start.y;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x, y), ImVec2(x, y + line_height),
                IM_COL32(255, 255, 255, 255), 1.5f
            );
        }

        if (has_selection_) {
            CursorPosition sel_start = std::min(cursor_, selection_start_);
            CursorPosition sel_end = std::max(cursor_, selection_start_);

            if (lineNo >= sel_start.line && lineNo <= sel_end.line) {
                int begin_col = (lineNo == sel_start.line) ? sel_start.column : 0;
                int end_col = (lineNo == sel_end.line) ? sel_end.column : static_cast<int>(line.size());

                if (begin_col < end_col) {
                    std::string segment = SafeSubstr(line, begin_col, end_col - begin_col);

                    ImVec2 sel_start_pos = text_start;
                    sel_start_pos.x += ImGui::CalcTextSize(SafeSubstr(line, 0, begin_col).c_str()).x;

                    ImVec2 sel_end_pos = sel_start_pos;
                    sel_end_pos.x += ImGui::CalcTextSize(segment.c_str()).x;
                    sel_end_pos.y += line_height;

                    ImGui::GetWindowDrawList()->AddRectFilled(sel_start_pos, sel_end_pos,
                        IM_COL32(100, 100, 255, 80));
                }
            }
        }

        auto lineTokens = GetVisibleTokensForLine(lineNo);

        int col = 0;
        for (const auto& tok : lineTokens) {
            if (tok.column < col) continue;

            if (tok.column > col) {
                std::string text = SafeSubstr(line, col, tok.column - col);
                ImGui::TextUnformatted(text.c_str());
                ImGui::SameLine(0, 0);
            }

            ImVec4 color = tok.color;
            auto sem_it = local_sem_kind.find({ tok.line, tok.column });
            if (sem_it != local_sem_kind.end()) {
                color = GetSemanticColor(sem_it->second);
            }

            int tok_end = tok.column + tok.length;
            if (tok_end > visible_column_start_ && tok.column < visible_column_start_ + visible_column_width_) {
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(SafeSubstr(line, tok.column, tok.length).c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 0);
            }

            col = tok_end;
        }

        if (col < static_cast<int>(line.size())) {
            ImGui::TextUnformatted(SafeSubstr(line, col).c_str());
            ImGui::SameLine(0, 0);
        }

        ImGui::NewLine();
    }

    int remaining_lines = static_cast<int>(lines_.size()) - end_line;
    if (remaining_lines > 0) {
        float skip_height = remaining_lines * ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + skip_height);
    }
    ImGui::SetWindowFontScale(1.0f);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("Minimap", ImVec2(minimapW, 0), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawMinimap();
    
    ImGui::EndChild();
}

void TextEditor::SelectWordAt(const CursorPosition& pos)
{
    if (pos.line >= lines_.size()) return;
    const std::string& line = lines_[pos.line];
    if (pos.column >= line.size()) return;

    auto isWord = [](char c) {
        return std::isalnum((unsigned char)c) || c == '_' || c == '-';
        };
    if (!isWord(line[pos.column])) return;

    int start = pos.column;
    int end = pos.column + 1;
    while (start > 0 && isWord(line[start - 1]))           --start;
    while (end < (int)line.size() && isWord(line[end])) ++end;

    selection_start_ = { pos.line, start };
    cursor_ = { pos.line, end };
    has_selection_ = true;

    DBG_TEDITOR(DebugModule::SELECTION, "SelectWord",
        "line %d  col %d-%d  text=\"%s\"",
        pos.line, start, end,
        SafeSubstr(line, start, end - start).c_str());
}
void TextEditor::SelectLineAt(int lineIdx)
{
    if (lineIdx >= lines_.size()) return;

    selection_start_ = { lineIdx, 0 };
    cursor_ = { lineIdx, (int)lines_[lineIdx].size() };
    has_selection_ = true;

    DBG_TEDITOR(DebugModule::SELECTION, "SelectLine",
        "line %d selected (length=%zu)",
        lineIdx, lines_[lineIdx].size());
}