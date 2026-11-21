// User prompts (TaskDialogIndirect) and file dialogs (IFileOpenDialog / IFileSaveDialog).
#include "app.hpp"

#include <commctrl.h>
#include <shobjidl.h>

#include <string>

namespace pxc::shell {

namespace {

int taskDialog(HWND owner, const std::wstring& title, const std::wstring& main, const std::wstring& content,
               const TASKDIALOG_BUTTON* buttons, UINT count, int defaultId, PCWSTR icon) {
    TASKDIALOGCONFIG c{sizeof(c)};
    c.hwndParent = owner;
    c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    c.pszWindowTitle = title.c_str();
    c.pszMainInstruction = main.c_str();
    c.pszContent = content.empty() ? nullptr : content.c_str();
    c.pButtons = buttons;
    c.cButtons = count;
    c.nDefaultButton = defaultId;
    c.pszMainIcon = icon;
    int pressed = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&c, &pressed, nullptr, nullptr))) return IDCANCEL;
    return pressed;
}

constexpr int kSave = 1001, kDontSave = 1002, kOpenReplace = 1003;

std::optional<std::filesystem::path> runFileDialog(HWND owner, bool save, const std::filesystem::path& suggested) {
    IFileDialog* dlg = nullptr;
    const HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr,
                                        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return std::nullopt;
    const COMDLG_FILTERSPEC filters[] = {
        {L"Text documents (*.txt)", L"*.txt"},
        {L"HTML documents (*.html; *.htm)", L"*.html;*.htm"},
        {L"All files (*.*)", L"*.*"},
    };
    dlg->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dlg->SetFileTypeIndex(1);
    if (save) {
        dlg->SetDefaultExtension(L"txt");
        dlg->SetFileName(suggested.filename().c_str());
    }
    std::optional<std::filesystem::path> out;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                out = std::filesystem::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    return out;
}

} // namespace

editor::PromptChoice PromptService::askSaveChanges(std::wstring_view documentName) {
    if (app().benchMode) return editor::PromptChoice::DontSave; // bench scripts auto-answer "Don't Save"
    const TASKDIALOG_BUTTON buttons[] = {{kSave, L"&Save"}, {kDontSave, L"Do&n't Save"}, {IDCANCEL, L"Cancel"}};
    const int r = taskDialog(*owner_, L"PixelCSS", L"Save changes to " + std::wstring(documentName) + L"?", L"",
                             buttons, 3, kSave, nullptr);
    if (r == kSave) return editor::PromptChoice::Save;
    if (r == kDontSave) return editor::PromptChoice::DontSave;
    return editor::PromptChoice::Cancel;
}

editor::InvalidEncodingChoice PromptService::askInvalidEncoding(std::wstring_view fileName) {
    const TASKDIALOG_BUTTON buttons[] = {{kOpenReplace, L"Open with replacement (U+FFFD)"}, {IDCANCEL, L"Cancel"}};
    const int r = taskDialog(*owner_, L"PixelCSS", std::wstring(fileName) + L" is not valid UTF-8 text.",
                             L"Invalid byte sequences can be replaced with U+FFFD. The document will be marked as "
                             L"converted and saved as UTF-8.",
                             buttons, 2, IDCANCEL, TD_WARNING_ICON);
    return r == kOpenReplace ? editor::InvalidEncodingChoice::OpenWithReplacement : editor::InvalidEncodingChoice::Cancel;
}

bool PromptService::confirmConvertToUtf8(std::wstring_view fileName) {
    const TASKDIALOG_BUTTON buttons[] = {{IDOK, L"Save as UTF-8"}, {IDCANCEL, L"Cancel"}};
    return taskDialog(*owner_, L"PixelCSS", L"Save " + std::wstring(fileName) + L" as UTF-8?",
                      L"This document was opened as UTF-16. PixelCSS saves plain text as UTF-8.", buttons, 2, IDOK,
                      TD_INFORMATION_ICON) == IDOK;
}

void PromptService::showError(std::wstring_view message) {
    if (app().benchMode) return;
    MessageBoxW(*owner_, std::wstring(message).c_str(), L"PixelCSS", MB_ICONERROR | MB_OK);
}

std::optional<std::filesystem::path> FileDialogs::pickOpen() { return runFileDialog(*owner_, false, {}); }
std::optional<std::filesystem::path> FileDialogs::pickSave(const std::filesystem::path& suggested) {
    return runFileDialog(*owner_, true, suggested);
}

} // namespace pxc::shell
