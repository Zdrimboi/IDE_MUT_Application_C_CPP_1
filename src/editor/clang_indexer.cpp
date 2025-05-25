#include "clang_indexer.h"
#include <clang-c/Index.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>
#include <cstdarg>

#define DEBUG_CLANGINDEXER
#ifdef DEBUG_CLANGINDEXER
#include <cstdio>

// ANSI color codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\x1b[1m"

enum class DebugModule {
    INDEXER,
    CACHE,
    PARSE,
    AST,
    CLEANUP
};

static const char* GetModuleName(DebugModule mod) {
    switch (mod) {
    case DebugModule::INDEXER: return "INDEXER";
    case DebugModule::CACHE:   return "CACHE";
    case DebugModule::PARSE:   return "PARSE";
    case DebugModule::AST:     return "AST";
    case DebugModule::CLEANUP: return "CLEANUP";
    default:                   return "UNKNOWN";
    }
}

static const char* GetModuleColor(DebugModule mod) {
    switch (mod) {
    case DebugModule::INDEXER: return ANSI_COLOR_BLUE;
    case DebugModule::CACHE:   return ANSI_COLOR_MAGENTA;
    case DebugModule::PARSE:   return ANSI_COLOR_GREEN;
    case DebugModule::AST:     return ANSI_COLOR_CYAN;
    case DebugModule::CLEANUP: return ANSI_COLOR_YELLOW;
    default:                   return ANSI_COLOR_RESET;
    }
}

static void DebugPrint(DebugModule mod, const char* action, const char* fmt, ...) {
    // timestamp
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm* lt = localtime(&t);
    fprintf(stderr, "[%02d:%02d:%02d.%03d] ", lt->tm_hour, lt->tm_min, lt->tm_sec, (int)ms.count());

    // module
    fprintf(stderr, "%s%s[%-9s]%s ", ANSI_COLOR_BOLD, GetModuleColor(mod), GetModuleName(mod), ANSI_COLOR_RESET);

    // action
    fprintf(stderr, "%s%-12s%s ", ANSI_COLOR_BOLD, action, ANSI_COLOR_RESET);

    // message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#define DBG_CINDEX(mod, action, fmt, ...) \
    do { DebugPrint(mod, action, fmt, ##__VA_ARGS__); } while(0)
#else
#define DBG_CINDEX(mod, action, fmt, ...) ((void)0)
#endif

// clang_indexer.cpp

// Global index cache
static CXIndex g_clang_index = nullptr;
static std::mutex g_index_mutex;

static std::unordered_map<std::size_t, CXTranslationUnit> g_tu_cache_;
static std::mutex                            g_tu_mutex_;

std::vector<Symbol> ClangIndexer::Index(const std::string& filepath,
    const std::string& code) {
    std::vector<Symbol> symbols;
    DBG_CINDEX(DebugModule::INDEXER, "Index", "Indexing '%s' (%zu bytes)", filepath.c_str(), code.size());

    // Acquire or create index
    CXIndex index;
    {
        std::lock_guard<std::mutex> lock(g_index_mutex);
        if (!g_clang_index) {
            DBG_CINDEX(DebugModule::INDEXER, "CreateIndex", "Creating new CXIndex");
            g_clang_index = clang_createIndex(0, 0);
        }
        else {
            DBG_CINDEX(DebugModule::INDEXER, "ReuseIndex", "Using existing CXIndex");
        }
        index = g_clang_index;
    }

    // Build arguments
    DBG_CINDEX(DebugModule::PARSE, "BuildArgs", "Building command-line arguments");
    std::vector<const char*> args;
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    bool isC = (ext == "c");
    args.push_back(isC ? "-xc" : "-xc++");
    args.push_back(isC ? "-std=c17" : "-std=c++17");
    // ... include paths ...
    args.push_back("-IC:/Program Files/LLVM/lib/clang/17.0.0/include");
    // more -I flags omitted for brevity

    // Prepare unsaved file
    CXUnsavedFile unsaved{ filepath.c_str(), code.c_str(), code.size() };
    DBG_CINDEX(DebugModule::PARSE, "UnsavedFile", "Filename='%s', Length=%zu", unsaved.Filename, unsaved.Length);

    // Parse or reparse TU
    size_t key = std::hash<std::string>{}(filepath + "\n" + code);
    CXTranslationUnit tu = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_tu_mutex_);
        auto it = g_tu_cache_.find(key);
        if (it != g_tu_cache_.end()) {
            tu = it->second;
            DBG_CINDEX(DebugModule::CACHE, "CacheHit", "TU cache hit for key %zx", key);
            unsigned opts = clang_defaultEditingTranslationUnitOptions();
            if (clang_reparseTranslationUnit(tu, 1, &unsaved, opts) != 0) {
                DBG_CINDEX(DebugModule::CACHE, "ReparseFail", "Reparse failed, disposing TU");
                clang_disposeTranslationUnit(tu);
                g_tu_cache_.erase(it);
                tu = nullptr;
            }
            else {
                DBG_CINDEX(DebugModule::CACHE, "ReparsedTU", "Reparsed TU successfully");
            }
        }
        if (!tu) {
            DBG_CINDEX(DebugModule::PARSE, "ParseTU", "Parsing new TU");
            tu = clang_parseTranslationUnit(
                index,
                filepath.c_str(),
                args.data(), static_cast<int>(args.size()),
                &unsaved, 1,
                CXTranslationUnit_DetailedPreprocessingRecord
            );
            if (!tu) {
                DBG_CINDEX(DebugModule::PARSE, "ParseFail", "Failed to parse TU for %s", filepath.c_str());
                return symbols;
            }
            g_tu_cache_[key] = tu;
            DBG_CINDEX(DebugModule::CACHE, "CacheInsert", "Inserted TU into cache, size=%zu", g_tu_cache_.size());
        }
    }

    // Walk the AST
    DBG_CINDEX(DebugModule::AST, "VisitRoot", "Walking AST");
    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root,
        [](CXCursor c, CXCursor, CXClientData client_data) {
            auto& out = *reinterpret_cast<std::vector<Symbol>*>(client_data);
            CXSourceLocation loc = clang_getCursorLocation(c);
            if (!clang_Location_isFromMainFile(loc))
                return CXChildVisit_Continue;
            CXCursorKind kind = clang_getCursorKind(c);
            CXString spelling = clang_getCursorSpelling(c);
            CXString kindStr = clang_getCursorKindSpelling(kind);
            unsigned line, col;
            clang_getSpellingLocation(loc, nullptr, &line, &col, nullptr);
            out.push_back({ clang_getCString(spelling), static_cast<int>(line), static_cast<int>(col), clang_getCString(kindStr) });
            clang_disposeString(kindStr);
            clang_disposeString(spelling);
            DBG_CINDEX(DebugModule::AST, "Symbol", "%s at %d:%d", clang_getCString(spelling), line, col);
            return CXChildVisit_Recurse;
        }, &symbols);
    DBG_CINDEX(DebugModule::AST, "VisitDone", "Collected %zu symbols", symbols.size());

    return symbols;
}

void ClangIndexer::Cleanup() {
    DBG_CINDEX(DebugModule::CLEANUP, "CleanupStart", "Disposing all cached TUs and CXIndex");
    {
        std::lock_guard<std::mutex> lock(g_tu_mutex_);
        for (auto& kv : g_tu_cache_) {
            clang_disposeTranslationUnit(kv.second);
        }
        g_tu_cache_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_index_mutex);
        if (g_clang_index) {
            clang_disposeIndex(g_clang_index);
            g_clang_index = nullptr;
            DBG_CINDEX(DebugModule::CLEANUP, "IndexDisposed", "CXIndex disposed");
        }
    }
    DBG_CINDEX(DebugModule::CLEANUP, "CleanupDone", "Cleanup complete");
}
