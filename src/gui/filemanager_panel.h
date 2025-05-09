#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <imgui.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------------------------
// helper: portable UTF‑8 conversion (MSVC char8_t workaround)
// ---------------------------------------------------------------------------------------------
inline std::string pathToUtf8(const fs::path& p)
{
#if defined(_MSC_VER) && defined(__cpp_char8_t)
    std::u8string tmp = p.u8string();
    return std::string(tmp.begin(), tmp.end());
#else
    return p.u8string();
#endif
}

/**
 * FileExplorer – minimal pane that shows a tree view of the local file‑system and
 * notifies you when the user picks a file or directory.**/
class FileExplorer
{
public:
    FileExplorer() = default;
    explicit FileExplorer(const fs::path& root) { setRootPath(root); }

    void setRootPath(const fs::path& root) { m_root = fs::absolute(root); }
    void setSelectionCallback(std::function<void(const fs::path&)> cb) { m_onSelect = std::move(cb); }

    // -----------------------------------------------------------------------------------------------------------------
    void draw(const char* title = "File Explorer")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        const std::string rootStr = pathToUtf8(m_root);
        ImGui::TextUnformatted(rootStr.c_str());
        ImGui::Separator();

        // full‑size scrolling region
        ImGui::BeginChild("##file_tree", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        drawDirectory(m_root);
        ImGui::EndChild();

        ImGui::End();
    }

private:
    fs::path                                    m_root;
    std::function<void(const fs::path&)>        m_onSelect;

    // -----------------------------------------------------------------------------------------------------------------
    void drawDirectory(const fs::path& dir)
    {
        // guard against permission errors on the directory itself
        if (!fs::exists(dir)) return;

        std::string label = pathToUtf8(dir.filename());
        if (label.empty()) label = pathToUtf8(dir.root_name().empty() ? dir.string() : dir.root_name());

        ImGui::PushID(label.c_str()); // ensure unique ID even if two sibling folders share a name elsewhere

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

        if ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
            (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        {
            if (m_onSelect) m_onSelect(dir);
        }

        if (opened)
        {
            try {
                for (auto& e : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied))
                {
                    if (e.is_directory()) drawDirectory(e.path());
                    else                  drawFile(e.path());
                }
            }
            catch (const fs::filesystem_error&) { /* skip inaccessible sub‑dirs */ }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // -----------------------------------------------------------------------------------------------------------------
    void drawFile(const fs::path& file)
    {
        std::string label = pathToUtf8(file.filename());
        ImGui::PushID(label.c_str());
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
        ImGui::TreeNodeEx(label.c_str(), flags);

        if ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
            (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        {
            if (m_onSelect) m_onSelect(file);
        }
        ImGui::PopID();
    }
};
