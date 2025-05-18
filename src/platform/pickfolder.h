// PickFolder.h/ PickFolder.cpp  ────────────────────────────────
#pragma once
#include <filesystem>
#include <optional>
#include <shobjidl.h>   // IFileOpenDialog
#include <windows.h>

inline std::optional<std::filesystem::path> PickFolder(HWND owner = nullptr)
{
    // Make sure COM is initialised on this thread once in your program
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitHere = SUCCEEDED(hr);

    IFileDialog* dlg = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr)) { if (comInitHere) CoUninitialize();  return std::nullopt; }

    // Turn it into a folder-picker
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
        FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);

    if (SUCCEEDED(dlg->Show(owner)))                    // user clicked “OK”
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)))
        {
            PWSTR widePath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath)))
            {
                std::filesystem::path folder{ widePath };
                CoTaskMemFree(widePath);
                item->Release();
                dlg->Release();
                if (comInitHere) CoUninitialize();
                return folder;
            }
            item->Release();
        }
    }
    dlg->Release();
    if (comInitHere) CoUninitialize();
    return std::nullopt;                                // user cancelled
}
