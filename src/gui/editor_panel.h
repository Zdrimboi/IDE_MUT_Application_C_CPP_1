// EditorPanel.hpp ────────────────────────────────────────────────────────────
// • ImGui‐based code editor with Tree-sitter C/C++ syntax colouring.
// • Minimal typing (add/delete char), line numbers, scrollable, tabbed files.
// • No runtime crashes – uses ts_parser_parse_string_encoding (no custom TSInput).
//
// Requirements
//   imgui >= 1.89  (and IMGUI_DEFINE_MATH_OPERATORS enabled)
//   Tree-sitter runtime + C++ grammar objects
//     ├─ tree-sitter-cpp/src/parser.c
//     └─ tree-sitter-cpp/src/scanner.cc
//   Build both objects and link them plus the core tree-sitter library.
//
// ---------------------------------------------------------------------------
#pragma once

// ImGui math operators (ImVec2 + ImVec2 etc.)
#include <imgui.h>
#include <imgui_internal.h>

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
static inline ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
    return { a.x + b.x, a.y + b.y };
}

// ── Tree-sitter C API & C++ language symbol
extern "C" {
#include <tree_sitter/api.h>
    const TSLanguage* tree_sitter_cpp();
}

namespace ed {

    // ──────────────────────────────────────────────────────────────────────────
    // Helper types
    // ──────────────────────────────────────────────────────────────────────────
    struct Span {                 // a coloured token on one visual line
        uint32_t start;           // starting column (byte offset in UTF-8)
        uint32_t end;             // end column (exclusive) or UINT32_MAX for “to EOL”
        ImU32    colour;          // ImGui RGBA
    };

    static constexpr float LINE_PAD = 2.0f;  // vertical spacing
    static constexpr float GUTTER_WIDTH = 46.0f; // space for line numbers

    // ──────────────────────────────────────────────────────────────────────────
    // Tree-sitter wrapper (parser + query + cursor)
    // ──────────────────────────────────────────────────────────────────────────
    struct TSWrap {
        TSParser* parser = nullptr;
        TSTree* tree = nullptr;
        TSQuery* query = nullptr;
        TSQueryCursor* cursor = nullptr;

        TSWrap() {
            parser = ts_parser_new();
            ts_parser_set_language(parser, tree_sitter_cpp());

            // tiny highlight query (comment / string / number / type / func / ident)
            static const char* Q = R"(
            (comment)                 @c
            (string_literal)          @s
            (number_literal)          @n
            (type_identifier)         @t
            (call_expression
               function: (identifier) @f)
            (identifier)              @i
        )";
            uint32_t offset; TSQueryError err;
            query = ts_query_new(tree_sitter_cpp(), Q,
                (uint32_t)strlen(Q), &offset, &err);
            cursor = ts_query_cursor_new();
        }

        ~TSWrap() {
            if (tree) ts_tree_delete(tree);
            ts_query_cursor_delete(cursor);
            ts_query_delete(query);
            ts_parser_delete(parser);
        }
    };

    // ──────────────────────────────────────────────────────────────────────────
    // EditorPanel  –  multiple tabs of “File” objects
    // ──────────────────────────────────────────────────────────────────────────
    class EditorPanel {
    public:
        // open a file from disk
        void openFile(const std::filesystem::path& p) {
            std::ifstream fs(p, std::ios::binary);
            if (!fs) return;
            std::string txt((std::istreambuf_iterator<char>(fs)), {});
            files.emplace_back(nextId++, p.string(), std::move(txt));
            focusNext = files.back().id;
        }

        // main ImGui draw call
        void draw(const char* title = "Editor") {
            if (!ImGui::Begin(title)) { ImGui::End(); return; }

            if (files.empty()) {
                ImGui::TextDisabled("No files open");
                ImGui::End();
                return;
            }

            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_Reorderable)) {
                int closeIdx = -1;

                for (int i = 0; i < (int)files.size(); ++i) {
                    File& f = files[i];
                    ImGuiTabItemFlags flags = f.dirty ? ImGuiTabItemFlags_UnsavedDocument : 0;
                    if (f.id == focusNext) flags |= ImGuiTabItemFlags_SetSelected;

                    bool tabOpen = true;
                    if (ImGui::BeginTabItem(f.displayName().c_str(), &tabOpen, flags)) {
                        activeId = f.id;
                        f.render();
                        ImGui::EndTabItem();
                    }
                    if (!tabOpen) closeIdx = i;
                }

                ImGui::EndTabBar();
                if (closeIdx >= 0) files.erase(files.begin() + closeIdx);
            }
            focusNext = -1;
            ImGui::End();
        }

    private:
        // ──────────────────────────────────────────────────────────────────
        //  nested File struct – one open buffer
        // ──────────────────────────────────────────────────────────────────
        struct File {
            // core data
            std::string path;
            std::string buf;
            std::string original;
            int  id;
            bool dirty = false;

            // helpers
            std::vector<size_t>            lineStart; // byte offset per line
            std::vector<std::vector<Span>> spans;     // colour spans per line
            TSWrap ts;

            File(int iid, std::string p, std::string b)
                : path(std::move(p)), buf(std::move(b)), original(buf), id(iid)
            {
                indexLines();
                parse();
            }

            // file name to show on tab
            std::string displayName() const {
                return path.empty() ? "Untitled"
                    : std::filesystem::path(path).filename().string();
            }

            // rebuild lineStart table
            void indexLines() {
                lineStart.clear();
                lineStart.push_back(0);
                for (size_t i = 0; i < buf.size(); ++i)
                    if (buf[i] == '\n') lineStart.push_back(i + 1);
            }

            // parse buffer with Tree-sitter
            void parse() {
                TSTree* nt = ts_parser_parse_string_encoding(
                    ts.parser,
                    ts.tree,
                    buf.c_str(),
                    (uint32_t)buf.size(),
                    TSInputEncodingUTF8);

                if (ts.tree) ts_tree_delete(ts.tree);
                ts.tree = nt;
                buildSpanCache();
            }

            // build colour-span cache using highlight query
            void buildSpanCache() {
                spans.assign(lineStart.size(), {});
                ts_query_cursor_exec(ts.cursor, ts.query, ts_tree_root_node(ts.tree));

                TSQueryMatch m;
                while (ts_query_cursor_next_match(ts.cursor, &m)) {
                    for (uint32_t c = 0; c < m.capture_count; ++c) {
                        const auto& cap = m.captures[c];
                        TSPoint s = ts_node_start_point(cap.node);
                        TSPoint e = ts_node_end_point(cap.node);
                        ImU32 col = colour(cap.index);

                        if (s.row == e.row) {
                            spans[s.row].push_back({ s.column, e.column, col });
                        }
                        else {
                            spans[s.row].push_back({ s.column, UINT32_MAX, col });
                            for (uint32_t r = s.row + 1; r < e.row; ++r)
                                spans[r].push_back({ 0, UINT32_MAX, col });
                            spans[e.row].push_back({ 0, e.column, col });
                        }
                    }
                }
            }

            // colour palette
            static ImU32 colour(uint32_t cap) {
                switch (cap) {
                case 0: return IM_COL32(128, 128, 128, 255);  // comment
                case 1: return IM_COL32(220, 150, 130, 255);  // string
                case 2: return IM_COL32(180, 220, 130, 255);  // number
                case 3: return IM_COL32(96, 175, 255, 255);  // type
                case 4: return IM_COL32(200, 200, 110, 255);  // func
                default:return IM_COL32_WHITE;
                }
            }

            // ImGui rendering
            void render() {
                if (lineStart.empty()) {
                    ImGui::TextDisabled("// empty file");
                    ImGui::EndChild();
                    return;
                }
                // simple typing: append char / backspace
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                    auto& io = ImGui::GetIO();
                    for (ImWchar ch : io.InputQueueCharacters) {
                        if (ch == '\b') { if (!buf.empty()) buf.pop_back(); }
                        else { buf.push_back((char)ch); }
                        dirty = true;
                    }
                    io.InputQueueCharacters.resize(0);
                }
                if (dirty) { indexLines(); parse(); dirty = false; }

                ImGui::BeginChild("##code",
                    ImVec2(0, 0),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 origin = ImGui::GetCursorScreenPos();
                float lineH = ImGui::GetTextLineHeight() + LINE_PAD;

                ImGuiListClipper clip;
                clip.Begin(static_cast<int>(lineStart.size()),
                    ImGui::GetTextLineHeight() + LINE_PAD);   // << explicit item height
                while (clip.Step()) {
                    for (int l = clip.DisplayStart; l < clip.DisplayEnd; ++l) {
                        size_t sb = lineStart[l];
                        size_t eb = (l + 1 < lineStart.size() ? lineStart[l + 1] : buf.size());
                        std::string_view ln(&buf[sb], eb - sb);

                        // line number
                        char num[8]; sprintf(num, "%4d", l + 1);
                        dl->AddText(
                            origin + ImVec2(0, l * lineH),
                            IM_COL32(150, 150, 150, 255),
                            num);

                        // coloured text
                        float  x = origin.x + GUTTER_WIDTH;
                        size_t cur = 0;
                        for (const auto& sp : spans[l]) {
                            size_t segEnd = std::min<size_t>(sp.start, ln.size());
                            if (segEnd > cur) {
                                auto sv = ln.substr(cur, segEnd - cur);
                                dl->AddText(
                                    { x, origin.y + l * lineH },
                                    IM_COL32_WHITE,
                                    sv.data(), sv.data() + sv.size());
                                x += ImGui::CalcTextSize(sv.data(), sv.data() + sv.size()).x;
                            }
                            size_t end = (sp.end == UINT32_MAX ? ln.size() : sp.end);
                            if (end > cur) {
                                auto sv = ln.substr(sp.start, end - sp.start);
                                dl->AddText(
                                    { x, origin.y + l * lineH },
                                    sp.colour,
                                    sv.data(), sv.data() + sv.size());
                                x += ImGui::CalcTextSize(sv.data(), sv.data() + sv.size()).x;
                            }
                            cur = end;
                        }
                        if (cur < ln.size()) {
                            dl->AddText(
                                { x, origin.y + l * lineH },
                                IM_COL32_WHITE,
                                ln.data() + cur, ln.data() + ln.size());
                        }
                    }
                }
                clip.End();
                ImGui::EndChild();
            }
        };

        // panel state
        std::vector<File> files;
        int nextId = 1;
        int focusNext = -1;
        int activeId = -1;
    };

} // namespace ed
