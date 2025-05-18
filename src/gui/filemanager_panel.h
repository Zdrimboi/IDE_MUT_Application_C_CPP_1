#pragma once

// ---------------------------------------------------------------------------------------------------------------------
// Dear ImGui File‑Manager / Explorer panel – context‑menu‑only, with proper selection & highlighting
// ---------------------------------------------------------------------------------------------------------------------

#include <filesystem>
#include <string>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <imgui.h>

namespace fs = std::filesystem;

inline std::string pathToUtf8(const fs::path& p)
{
#if defined(_MSC_VER) && defined(__cpp_char8_t)
    std::u8string tmp = p.u8string();
    return std::string(tmp.begin(), tmp.end());
#else
    return p.u8string();
#endif
}

class FileManagerPanel
{
public:
    explicit FileManagerPanel(const fs::path& root = fs::current_path())
        : m_root(fs::absolute(root)), m_selectedPath(m_root) {
    }

    void setRoot(const fs::path& root)
    {
        m_root = fs::absolute(root);
        m_selectedPath = m_root;
    }

	void GetRoot(fs::path& root) const
	{
		root = m_root;
	}

    void setOpenFileCallback(std::function<void(const fs::path&)> cb) { m_openFileCB = std::move(cb); }

    // -----------------------------------------------------------------------------
    void draw(const char* title = "File Manager")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        ImGui::BeginChild("##file_tree", ImVec2(0, 0), true,
            ImGuiWindowFlags_HorizontalScrollbar);
        drawDirectory(m_root);
        ImGui::EndChild();

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered())
            m_selectedPath = m_root;

        /* ---------------- NEW ---------------- */
        if (m_modalNextFrame != Modal::None)
        {
            m_activeModal = m_modalNextFrame;
            m_modalNextFrame = Modal::None;
            ImGui::OpenPopup(modalTitle(m_activeModal));
        }
        /* ------------------------------------- */

        handlePopups();
        ImGui::End();
    }


private:
    fs::path                         m_root;
    fs::path                         m_selectedPath;
    fs::path                         m_clipboardPath;
    fs::path                         m_pasteTargetDir;
    bool                             m_clipboardCut = false;
    std::function<void(const fs::path&)> m_openFileCB;

    enum class Modal { None, ConfirmDelete, Rename, NewFolder, NewFile, NameConflict };
    Modal                            m_activeModal = Modal::None;
    Modal                            m_modalNextFrame = Modal::None;
    char                             m_inputBuffer[260]{};

    // -----------------------------------------------------------------------------
    void popupNameConflict()
    {
        if (ImGui::BeginPopupModal(modalTitle(Modal::NameConflict), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("An item named '%s' already exists.\n"
                "Choose a new name:", m_inputBuffer);
            ImGui::InputText("##newname", m_inputBuffer, sizeof(m_inputBuffer));

            if (ImGui::Button("Copy here", ImVec2(120, 0)))
            {
                fs::path dest = m_pasteTargetDir / m_inputBuffer;

                try {
                    if (m_clipboardCut)
                        fs::rename(m_clipboardPath, dest);
                    else if (fs::is_directory(m_clipboardPath))
                        fs::copy(m_clipboardPath, dest,
                            fs::copy_options::recursive |
                            fs::copy_options::overwrite_existing);
                    else
                        fs::copy_file(m_clipboardPath, dest,
                            fs::copy_options::overwrite_existing);
                }
                catch (const fs::filesystem_error& err) {
                    std::fprintf(stderr, "[FileManager] paste‑rename error: %s\n",
                        err.what());
                }

                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void drawDirectory(const fs::path& dir)
    {
        if (!fs::exists(dir)) return;

        std::string label = pathToUtf8(dir.filename());
        if (label.empty()) label = pathToUtf8(dir.root_name().empty() ? dir.string() : dir.root_name());

        ImGui::PushID(label.c_str());
        bool isSelected = (dir == m_selectedPath);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

        // Left click selects
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_selectedPath = dir;

        // Double‑click for selection (and expansion handled by ImGui)
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            m_selectedPath = dir;

        // Right‑click context menu – also selects
        if (ImGui::BeginPopupContextItem())
        {
            m_selectedPath = dir;
            directoryContextMenu(dir);
            ImGui::EndPopup();
        }

        if (opened)
        {
            try {
                std::vector<fs::directory_entry> entries;
                for (auto& e : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied))
                    entries.push_back(e);
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
                    {
                        if (a.is_directory() != b.is_directory()) return a.is_directory();
                        return a.path().filename() < b.path().filename();
                    });

                for (auto& e : entries)
                {
                    if (e.is_directory()) drawDirectory(e.path());
                    else                  drawFile(e.path());
                }
            }
            catch (const fs::filesystem_error& err)
            {
                std::fprintf(stderr, "[FileManager] directory_iterator error: %s\n", err.what());
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void drawFile(const fs::path& file)
    {
        std::string label = pathToUtf8(file.filename());
        ImGui::PushID(label.c_str());

        bool isSelected = (file == m_selectedPath);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked())
            m_selectedPath = file;

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            m_selectedPath = file;
            openInEditor();
        }

        if (ImGui::BeginPopupContextItem())
        {
            m_selectedPath = file;
            fileContextMenu(file);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // -----------------------------------------------------------------------------
    void directoryContextMenu(const fs::path& dir)
    {
        if (ImGui::MenuItem("New Folder")) openModal(Modal::NewFolder);
        if (ImGui::MenuItem("New File"))   openModal(Modal::NewFile);
        ImGui::Separator();
        if (ImGui::MenuItem("Copy"))   startCopy(false);
        if (ImGui::MenuItem("Cut"))    startCopy(true);
        if (ImGui::MenuItem("Paste"))  performPaste();
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) openModal(Modal::Rename);
        if (ImGui::MenuItem("Delete")) openModal(Modal::ConfirmDelete);
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Full Path"))    copyFullPath();
        if (ImGui::MenuItem("Copy Relative Path")) copyRelativePath();
        if (ImGui::MenuItem("Open in Explorer")) openInOSExplorer();
    }

    void fileContextMenu(const fs::path& file)
    {
        if (ImGui::MenuItem("Copy"))   startCopy(false);
        if (ImGui::MenuItem("Cut"))    startCopy(true);
        if (ImGui::MenuItem("Paste", nullptr, false, false)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) openModal(Modal::Rename);
        if (ImGui::MenuItem("Delete")) openModal(Modal::ConfirmDelete);
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Full Path"))    copyFullPath();
        if (ImGui::MenuItem("Copy Relative Path")) copyRelativePath();
        if (ImGui::MenuItem("Open in Explorer")) openInOSExplorer();
        if (ImGui::MenuItem("Open in Editor"))   openInEditor();
    }

    // -----------------------------------------------------------------------------
    void startCopy(bool cut)
    {
        if (m_selectedPath.empty()) return;
        m_clipboardPath = m_selectedPath;
        m_clipboardCut = cut;
    }

    void performPaste()
    {
        if (m_clipboardPath.empty() || m_selectedPath.empty()) return;

        fs::path targetDir = m_selectedPath;
        if (fs::is_regular_file(targetDir)) targetDir = targetDir.parent_path();

        fs::path dest = targetDir / m_clipboardPath.filename();

        if (fs::exists(dest))              // ---------- NEW ----------
        {
            m_pasteTargetDir = targetDir;  // remember where we’re pasting
            openModal(Modal::NameConflict);
            return;
        }

        try {
            if (m_clipboardCut)
                fs::rename(m_clipboardPath, dest);
            else if (fs::is_directory(m_clipboardPath))
                fs::copy(m_clipboardPath, dest,
                    fs::copy_options::recursive |
                    fs::copy_options::overwrite_existing);
            else
                fs::copy_file(m_clipboardPath, dest,
                    fs::copy_options::overwrite_existing);
        }
        catch (const fs::filesystem_error& err) {
            std::fprintf(stderr, "[FileManager] paste error: %s\n", err.what());
        }
    }


    void copyFullPath()
    {
        if (m_selectedPath.empty()) return;
        ImGui::SetClipboardText(pathToUtf8(m_selectedPath).c_str());
    }

    void copyRelativePath()
    {
        if (m_selectedPath.empty()) return;
        try {
            fs::path rel = fs::relative(m_selectedPath, m_root);
            ImGui::SetClipboardText(pathToUtf8(rel).c_str());
        }
        catch (...) {
            copyFullPath();
        }
    }

void openInOSExplorer()
{
    if (m_selectedPath.empty())
        return;

#if defined(_WIN32)
    // explorer /select highlights the item; fall back to plain open for directories
    std::string cmd;
    if (fs::is_directory(m_selectedPath))
        cmd = "explorer \"" + pathToUtf8(m_selectedPath) + "\"";
    else
        cmd = "explorer /select,\"" + pathToUtf8(m_selectedPath) + "\"";

#elif defined(__APPLE__)
    // Finder’s -R flag “reveals” (highlights) the file or folder
    std::string cmd = "open -R \"" + pathToUtf8(m_selectedPath) + "\"";

#else
    // No cross‑distro “reveal” flag on Linux/BSD; just open the containing folder
    fs::path folder = fs::is_directory(m_selectedPath)
                      ? m_selectedPath
                      : m_selectedPath.parent_path();
    std::string cmd = "xdg-open \"" + pathToUtf8(folder) + "\"";
#endif

    std::system(cmd.c_str());
}


    void openInEditor()
    {
        if (!m_openFileCB || m_selectedPath.empty() || fs::is_directory(m_selectedPath)) return;
        m_openFileCB(m_selectedPath);
    }

    // Modal helpers -------------------------------------------------------------------
    void openModal(Modal m)
    {
        // don’t call ImGui::OpenPopup() from inside another popup;
        // just mark it for the next frame.
        if ((m == Modal::ConfirmDelete || m == Modal::Rename) && m_selectedPath.empty())
            return;

        m_modalNextFrame = m;
        std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));

        if (m == Modal::Rename)
        {
            std::string name =
                (m == Modal::Rename) ? m_selectedPath.filename().string()
                : m_clipboardPath.filename().string();
            std::strncpy(m_inputBuffer, name.c_str(), sizeof(m_inputBuffer) - 1);
        }

        if (m == Modal::NameConflict)
        {
            std::string base = m_clipboardPath.stem().string();
            std::string ext = m_clipboardPath.extension().string();
            std::string suggested = base + "(1)" + ext;
            std::strncpy(m_inputBuffer, suggested.c_str(),
                sizeof(m_inputBuffer) - 1);
        }
    }


    const char* modalTitle(Modal m) const
    {
        switch (m)
        {
        case Modal::ConfirmDelete: return "Delete item?";
        case Modal::Rename:        return "Rename item";
        case Modal::NewFolder:     return "Create new folder";
        case Modal::NewFile:       return "Create new file";
        case Modal::NameConflict:   return "Name already exists";
        default:                   return "";
        }
    }

    void handlePopups()
    {
        switch (m_activeModal)
        {
        case Modal::ConfirmDelete:  popupDelete(); break;
        case Modal::Rename:         popupRename(); break;
        case Modal::NewFolder:      popupNewFolder(); break;
        case Modal::NewFile:        popupNewFile(); break;
        case Modal::NameConflict:   popupNameConflict(); break;
        default: break;
        }
    }

    void popupDelete()
    {
        if (ImGui::BeginPopupModal(modalTitle(Modal::ConfirmDelete), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Really delete '%s'?", pathToUtf8(m_selectedPath.filename()).c_str());
            ImGui::Separator();
            if (ImGui::Button("Yes", ImVec2(120, 0)))
            {
                try {
                    if (fs::is_directory(m_selectedPath))
                        fs::remove_all(m_selectedPath);
                    else
                        fs::remove(m_selectedPath);
                }
                catch (const fs::filesystem_error& err) {
                    std::fprintf(stderr, "[FileManager] delete error: %s\n", err.what());
                }
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0)))
            {
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void popupRename()
    {
        if (ImGui::BeginPopupModal(modalTitle(Modal::Rename), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("New name", m_inputBuffer, sizeof(m_inputBuffer));
            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                fs::path newPath = m_selectedPath.parent_path() / m_inputBuffer;
                try { fs::rename(m_selectedPath, newPath); }
                catch (const fs::filesystem_error& err) { std::fprintf(stderr, "[FileManager] rename error: %s\n", err.what()); }
                m_selectedPath = newPath;
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void popupNewFolder()
    {
        if (ImGui::BeginPopupModal(modalTitle(Modal::NewFolder), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Folder name", m_inputBuffer, sizeof(m_inputBuffer));
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                fs::path parent = m_selectedPath.empty() ? m_root : (fs::is_directory(m_selectedPath) ? m_selectedPath : m_selectedPath.parent_path());
                fs::path newDir = parent / m_inputBuffer;
                try { fs::create_directory(newDir); }
                catch (const fs::filesystem_error& err) { std::fprintf(stderr, "[FileManager] mkdir error: %s\n", err.what()); }
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void popupNewFile()
    {
        if (ImGui::BeginPopupModal(modalTitle(Modal::NewFile), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("File name", m_inputBuffer, sizeof(m_inputBuffer));
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                fs::path parent = m_selectedPath.empty() ? m_root : (fs::is_directory(m_selectedPath) ? m_selectedPath : m_selectedPath.parent_path());
                fs::path newFile = parent / m_inputBuffer;
                try {
                    std::ofstream(newFile.string()).close();
                }
                catch (...)
                {
                    std::fprintf(stderr, "[FileManager] could not create file\n");
                }
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_activeModal = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
};
