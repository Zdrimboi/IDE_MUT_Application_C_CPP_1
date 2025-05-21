#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <imgui.h>
#include "TextEditor.h"

/* ---------------------------------------------------------------------------
 *  EditorPanel  —  flicker-free tabs, persistent Save/Save-As,
 *                  automatic pinned-on-the-left, real-time dirty tracking
 * -------------------------------------------------------------------------*/
class EditorPanel
{
public:
    EditorPanel()
    {
        // prepare common language definition
        TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::CPlusPlus();
        // you can insert identifiers or preprocessor symbols here if needed
        m_language = lang;
    }

    /*--- Public API ---*/
    void openFile(const std::filesystem::path& path)
    {
        std::string fullPath = path.string();
        for (auto& f : m_files)
        {
            if (f.path == fullPath)
            {
                m_focus_next_id = f.id;          // focus the already-open tab next frame
                return;                          // already open
            }
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return;

        std::string content((std::istreambuf_iterator<char>(ifs)), {});

        FileEntry f;
        f.path = fullPath;
        f.id = m_nextId++;
        f.pinned = false;
        f.editor.SetLanguageDefinition(m_language);
        f.editor.SetText(content);
        f.original = content;
        m_files.push_back(std::move(f));
        resortPinned();                          // keep pins left

        m_focus_next_id = m_files.back().id;     // one-shot focus request
    }

    /*------------------------------------------------------------
     *  Main draw (call every frame)
     *----------------------------------------------------------*/
    void draw(const char* title = "Editor")
    {
        if (!ImGui::Begin(title)) { ImGui::End(); return; }

        if (m_files.empty())
        {
            ImGui::TextDisabled("No files open – drag a file here or use Ctrl+O");
            ImGui::End();
            return;
        }

        /*  Make sure pinned tabs stay on the left every frame.        */
        resortPinned();

        if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable))
        {
            int pendingClose = -1;

            for (int i = 0; i < (int)m_files.size(); ++i)
            {
                FileEntry& file = m_files[i];

                ImGuiTabItemFlags tif = 0;
                if (file.dirty) tif |= ImGuiTabItemFlags_UnsavedDocument;

                /* One-shot programmatic focus */
                if (file.id == m_focus_next_id)
                    tif |= ImGuiTabItemFlags_SetSelected;

                bool tabOpen = true;
                bool selected = ImGui::BeginTabItem(file.tabLabel().c_str(),
                    file.pinned ? nullptr : &tabOpen,
                    tif);

                if (!tabOpen && !file.pinned)
                    pendingClose = i; // close requested

                if (selected)
                {
                    /*  ►► This is the ONLY place we set the active id ◄◄  */
                    m_active_id = file.id;

                    // context menu
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Save", nullptr, false, file.dirty)) saveFile(file);
                        if (ImGui::MenuItem("Save As…"))                  saveFileAs(file);
                        if (ImGui::MenuItem("Open Containing Folder"))   openFolder(file);
                        ImGui::Separator();
                        if (ImGui::MenuItem(file.pinned ? "Unpin" : "Pin"))
                        {
                            file.pinned = !file.pinned;
                            resortPinned();
                        }
                        if (!file.pinned && ImGui::MenuItem("Close"))
                            pendingClose = i;
                        ImGui::EndPopup();
                    }

                    // --- TextEditor rendering ---
                    auto cpos = file.editor.GetCursorPosition();
                    // Use c_str() because Render expects const char*
                    file.editor.Render(file.tabLabel().c_str());
                    file.dirty = (file.editor.GetText() != file.original);

                    ImGui::EndTabItem();
                }

                /* Clear one-shot focus once processed */
                if (m_focus_next_id == file.id)
                    m_focus_next_id = -1;
            }
            ImGui::EndTabBar();

            if (pendingClose >= 0 && pendingClose < (int)m_files.size())
            {
                FileEntry& f = m_files[pendingClose];
                if (f.dirty)
                {
                    m_pendingCloseIndex = pendingClose;
                    ImGui::OpenPopup("UnsavedChanges");
                }
                else
                {
                    removeAt(pendingClose);
                }
            }
        }

        // unsaved changes popup same as before...
        if (m_pendingCloseIndex >= 0 && m_pendingCloseIndex < (int)m_files.size())
        {
            if (ImGui::BeginPopupModal("UnsavedChanges", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                FileEntry& file = m_files[m_pendingCloseIndex];
                ImGui::Text("The file '%s' has unsaved changes.",
                    file.displayName().c_str());
                ImGui::Text("What would you like to do?");
                ImGui::Separator();

                if (ImGui::Button("Save"))
                {
                    saveFile(file);
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Save As"))
                {
                    saveFileAs(file);
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save - Close"))
                {
                    removeAt(m_pendingCloseIndex);
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

private:
    /*------------------------------------------------------------
     *  FileEntry helper struct
     *----------------------------------------------------------*/
    struct FileEntry
    {
        std::string path;
        TextEditor editor;
        std::string original;
        int  id = 0;
        bool dirty = false;
        bool pinned = false;

        std::string displayName() const
        {
            if (path.empty()) return "Untitled";
            return std::filesystem::path(path).filename().string();
        }
        std::string tabLabel() const
        {
            std::string label;
            if (pinned) label += "\xF0\x9F\x93\x8C "; // 📌
            label += displayName();
            if (dirty) label += " *";
            label += "###" + std::to_string(id);
            return label;
        }
    };

    void resortPinned()
    {
        std::stable_partition(m_files.begin(), m_files.end(),
            [](const FileEntry& e) { return e.pinned; });
    }

    void removeAt(int idx)
    {
        if (idx < 0 || idx >= (int)m_files.size()) return;
        m_files.erase(m_files.begin() + idx);
        if (!m_files.empty())
            m_focus_next_id = m_files.front().id;
        else
            m_active_id = -1;
    }

    void saveFile(FileEntry& file)
    {
        if (file.path.empty()) { saveFileAs(file); return; }
        std::ofstream ofs(file.path, std::ios::binary);
        if (!ofs) return;
        auto txt = file.editor.GetText();
        ofs.write(txt.data(), txt.size());
        file.original = txt;
        file.dirty = false;
    }
    void saveFileAs(FileEntry& file)
    {
        if (file.path.empty()) file.path = "newfile.txt";
        saveFile(file);
    }
    void openFolder(const FileEntry& file)
    {
        if (file.path.empty()) return;
#ifdef _WIN32
        std::string cmd = "explorer /select,\"" + file.path + "\"";
#elif __APPLE__
        std::string cmd = "open -R \"" + file.path + "\"";
#else
        std::string cmd = "xdg-open \"" +
            std::filesystem::path(file.path).parent_path().string() + "\"";
#endif
        system(cmd.c_str());
    }

    std::vector<FileEntry> m_files;
    TextEditor::LanguageDefinition m_language;
    int  m_nextId = 1;
    int  m_active_id = -1;
    int  m_focus_next_id = -1;
    int  m_pendingCloseIndex = -1;
};