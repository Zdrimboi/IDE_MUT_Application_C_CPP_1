#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <imgui.h>

/* ---------------------------------------------------------------------------
 *  EditorPanel  —  flicker‑free tabs, persistent Save/Save‑As,
 *                  automatic pinned‑on‑the‑left, real‑time dirty tracking
 * -------------------------------------------------------------------------*/
class EditorPanel
{
public:
    /*‑‑‑ Public API ‑‑‑*/
    void openFile(const std::filesystem::path& path)
    {
        std::string fullPath = path.string();
        for (auto& f : m_files)
        {
            if (f.path == fullPath)
            {
                m_focus_next_id = f.id;          // focus the already‑open tab next frame
                return;                          // already open
            }
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return;

        std::string content((std::istreambuf_iterator<char>(ifs)), {});

        FileEntry f;
        f.path = fullPath;
        f.buffer = std::move(content);
        f.original = f.buffer;
        f.id = m_nextId++;
        m_files.push_back(std::move(f));
        resortPinned();                          // keep pins left

        m_focus_next_id = m_files.back().id;     // one‑shot focus request
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
        /*  (Any user drag that crosses the pin boundary is undone.)   */
        resortPinned();

        if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable))
        {
            int pendingClose = -1;

            for (int i = 0; i < (int)m_files.size(); ++i)
            {
                FileEntry& file = m_files[i];

                ImGuiTabItemFlags tif = 0;
                if (file.dirty) tif |= ImGuiTabItemFlags_UnsavedDocument;

                /* One‑shot programmatic focus */
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

                    // Right‑click context menu on tab header
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Save", nullptr, false, file.dirty)) saveFile(file);
                        if (ImGui::MenuItem("Save As…"))                            saveFileAs(file);
                        if (ImGui::MenuItem("Open Containing Folder"))             openFolder(file);
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

                    /* editable text area */
                    static auto resizeCb = [](ImGuiInputTextCallbackData* d)->int
                        {
                            if (d->EventFlag == ImGuiInputTextFlags_CallbackResize)
                            {
                                auto* str = static_cast<std::string*>(d->UserData);
                                str->resize(d->BufTextLen);
                                d->Buf = str->data();
                            }
                            return 0;
                        };

                    ImGuiInputTextFlags tf = ImGuiInputTextFlags_AllowTabInput
                        | ImGuiInputTextFlags_CallbackResize;

                    if (ImGui::InputTextMultiline("##editor",
                        file.buffer.data(),
                        file.buffer.size() + 1,
                        ImVec2(-FLT_MIN, -FLT_MIN),
                        tf, resizeCb, &file.buffer))
                    {
                        file.updateDirty();      // keeps dirty flag accurate even after Ctrl+Z
                    }

                    ImGui::EndTabItem();
                }

                /* Clear one‑shot focus once we've processed the tab */
                if (m_focus_next_id == file.id)
                    m_focus_next_id = -1;
            }
            ImGui::EndTabBar();

            /* handle tab close request */
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

        /* unsaved‑changes confirmation modal */
        if (m_pendingCloseIndex >= 0 && m_pendingCloseIndex < (int)m_files.size())
        {
            if (ImGui::BeginPopupModal("UnsavedChanges", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                FileEntry& file = m_files[m_pendingCloseIndex];
                ImGui::Text("The file '%s' has unsaved changes.",
                    file.displayName().c_str());
                ImGui::Text("What would you like to do?");
                ImGui::Separator();

                if (ImGui::Button("Save"))          // ⟵ now just saves, does NOT close
                {
                    saveFile(file);
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Save As"))       // ⟵ saves under new name, keeps open
                {
                    saveFileAs(file);
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save - Close"))
                {
                    removeAt(m_pendingCloseIndex);  // discard changes and close
                    m_pendingCloseIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_pendingCloseIndex = -1;       // abort close, keep tab
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
        std::string buffer;
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
            label += "###" + std::to_string(id);      // stable ID
            return label;
        }
        void updateDirty() { dirty = buffer != original; }
    };

    /*‑‑‑ utility helpers ‑‑‑*/
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
            m_focus_next_id = m_files.front().id; // ensure a valid active tab
        else
            m_active_id = -1;
    }

    /*‑‑‑ I/O ‑‑‑*/
    void saveFile(FileEntry& file)
    {
        if (file.path.empty()) { saveFileAs(file); return; }
        std::ofstream ofs(file.path, std::ios::binary);
        if (!ofs) return;
        ofs.write(file.buffer.data(), file.buffer.size());
        file.original = file.buffer;
        file.dirty = false;
    }
    void saveFileAs(FileEntry& file)
    {
        if (file.path.empty()) file.path = "newfile.txt"; // integrate a file‑picker here
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

    /*‑‑‑ data members ‑‑‑*/
    std::vector<FileEntry> m_files;
    int  m_nextId = 1;
    int  m_active_id = -1;   // set ONLY from ImGui selection
    int  m_focus_next_id = -1;   // one‑shot programmatic focus
    int  m_pendingCloseIndex = -1;   // index waiting for unsaved‑changes decision
};
