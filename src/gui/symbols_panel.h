#pragma once
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <clang_indexer.h>  // provides the Symbol struct

/*---------------------------------------------------------------------------
    SymbolsPanel – an "Outline"-style navigator similar to VS Code’s outline.
    Features:
      • Hierarchical tree built from fully-qualified names (split on "::").
      • Search-as-you-type filter (fuzzy substring, case-insensitive).
      • Double-click leaf or container to jump to definition (first child if container).
      • Column with kind string; resizable; scrollable.

    **Safety notes**
      • A dummy root node is now created in the constructor so the panel is
        safe to draw even _before_ `setSymbols()` is ever called.
      • All vector accesses are guarded by `assert`/range checks when
        `_ITERATOR_DEBUG_LEVEL` is enabled, eliminating the crash you saw in
        MSVC’s hardened STL (`vector subscript out of range`).
---------------------------------------------------------------------------*/

struct DisplaySymbol {
    std::string name;   // unqualified part
    std::string kind;   // "class", "method", …
    int         line;   // 1-based; 0 = container / unknown
    int         column; // 1-based; 0 = container / unknown
};

class SymbolsPanel {
public:
    using ActivateFn = std::function<void(int /*line*/, int /*column*/)>;

    SymbolsPanel() { initRoot(); }

    /*-----------------------------  Data feed  -----------------------------*/
    void setSymbols(const std::vector<Symbol>& syms)
    {
        initRoot();               // wipe & recreate the root (index 0)
        pathIndex_.clear();
        pathIndex_["<file-scope>"] = 0;

        // Build a tree using the fully-qualified name (split on "::").
        for (const auto& s : syms) {
            const std::string& full = s.name; // already owns its memory – safe for views

            // Split qualified name
            std::vector<std::string_view> parts;
            size_t pos = 0;
            while (true) {
                size_t next = full.find("::", pos);
                parts.emplace_back(full.data() + pos, next == std::string::npos ? full.size() - pos : next - pos);
                if (next == std::string::npos) break;
                pos = next + 2;
            }

            // Walk/create parent chain.
            std::string path;
            size_t parent = 0;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (!path.empty()) path += "::";
                path += parts[i];

                auto [it, inserted] = pathIndex_.try_emplace(path, 0);
                if (inserted) {
                    size_t idx = nodes_.size();
                    DisplaySymbol ds;
                    ds.name = std::string(parts[i]);
                    ds.kind = (i + 1 == parts.size()) ? s.kind : "";
                    ds.line = (i + 1 == parts.size()) ? s.line : 0;
                    ds.column = (i + 1 == parts.size()) ? s.column : 0;
                    nodes_.push_back({ std::move(ds), {} });
                    nodes_[parent].children.push_back(idx);
                    it->second = idx;
                }
                parent = it->second;
            }
        }

        // Stable sort children by source location so tree follows file order.
        auto byLocation = [&](size_t a, size_t b) {
            const auto& sa = nodes_[a].sym;
            const auto& sb = nodes_[b].sym;
            return std::tie(sa.line, sa.column, sa.name) < std::tie(sb.line, sb.column, sb.name);
            };
        std::function<void(size_t)> sortRec = [&](size_t n) {
            std::sort(nodes_[n].children.begin(), nodes_[n].children.end(), byLocation);
            for (size_t c : nodes_[n].children) sortRec(c);
            };
        sortRec(0);
    }

    void setActivateCallback(ActivateFn fn) { onActivate_ = std::move(fn); }

    /*------------------------------  Render  -------------------------------*/
    void draw(const char* title = "Symbols")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        // Early-out if there’s nothing to show yet (shouldn’t happen, but safe)
        if (nodes_.empty()) { ImGui::TextUnformatted("<no symbols>"); ImGui::End(); return; }

        // Search bar
        static char filterBuf[128] = "";
        ImGui::InputTextWithHint("##filter", "Filter symbols…", filterBuf, sizeof(filterBuf));
        std::string filter = filterBuf;
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return (char)std::tolower(c); });

        ImGui::Separator();

        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##symbols", 2, tableFlags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableHeadersRow();

            drawNodeRecursive(0, filter);
            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    struct Node {
        DisplaySymbol        sym;       // data for this node
        std::vector<size_t>  children;  // indices into nodes_
    };

    void initRoot()
    {
        nodes_.clear();
        nodes_.push_back({ {"<file-scope>", "", 0, 0}, {} }); // root (index 0)
    }

    void drawNodeRecursive(size_t idx, const std::string& filter)
    {
        assert(idx < nodes_.size());
        const Node& n = nodes_[idx];
        const bool isLeaf = n.children.empty();

        // Filter: skip subtrees that don’t match.
        if (!filter.empty() && !nodeMatches(idx, filter))
            return;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool open = ImGui::TreeNodeEx((void*)(intptr_t)idx, flags, "%s", n.sym.name.c_str());

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && onActivate_) {
            const DisplaySymbol* target = &n.sym;
            if (!isLeaf && !n.children.empty()) {
                const size_t firstChild = n.children.front();
                assert(firstChild < nodes_.size());
                target = &nodes_[firstChild].sym;
            }
            if (target->line > 0)
                onActivate_(target->line, target->column);
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(n.sym.kind.c_str());

        if (!isLeaf && open) {
            for (size_t c : n.children) drawNodeRecursive(c, filter);
            ImGui::TreePop();
        }
    }

    bool nodeMatches(size_t idx, const std::string& filter) const
    {
        const auto& name = nodes_[idx].sym.name;
        std::string low;
        low.resize(name.size());
        std::transform(name.begin(), name.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (low.find(filter) != std::string::npos)
            return true;
        for (size_t c : nodes_[idx].children)
            if (nodeMatches(c, filter))
                return true;
        return false;
    }

    std::vector<Node>                       nodes_;      // flat storage (0 = root)
    std::unordered_map<std::string, size_t> pathIndex_;
    ActivateFn                               onActivate_{};
};