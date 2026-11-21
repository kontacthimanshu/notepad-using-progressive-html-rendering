#pragma once
// Application shell state shared by the Win32 modules of PixelCSS.exe. Only the shell includes <windows.h>.

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pxc/editor/editor_controller.hpp"
#include "pxc/editor/file_workflow.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/html/document_node.hpp"
#include "pxc/layout/block_layout.hpp"
#include "pxc/render/framebuffer.hpp"
#include "pxc/storage/preferences.hpp"
#include "pxc/storage/win32_file_system.hpp"
#include "pxc/update/render_stats.hpp"

namespace pxc::shell {

// ---- Win32 implementations of the engine seams ----
class Win32Clipboard final : public editor::IClipboard {
public:
    explicit Win32Clipboard(HWND* owner) : owner_(owner) {}
    std::optional<std::u32string> getText() override;
    bool setText(std::u32string_view text) override;

private:
    HWND* owner_;
};

class PromptService final : public editor::IPromptService {
public:
    explicit PromptService(HWND* owner) : owner_(owner) {}
    editor::PromptChoice askSaveChanges(std::wstring_view documentName) override;
    editor::InvalidEncodingChoice askInvalidEncoding(std::wstring_view fileName) override;
    bool confirmConvertToUtf8(std::wstring_view fileName) override;
    void showError(std::wstring_view message) override;

private:
    HWND* owner_;
};

class FileDialogs final : public editor::IFileDialogs {
public:
    explicit FileDialogs(HWND* owner) : owner_(owner) {}
    std::optional<std::filesystem::path> pickOpen() override;
    std::optional<std::filesystem::path> pickSave(const std::filesystem::path& suggested) override;

private:
    HWND* owner_;
};

// Presents framebuffer rectangles through a DIB section + BitBlt (a pixel copy; no text APIs).
class GdiPresenter {
public:
    ~GdiPresenter();
    void resize(HDC reference, int w, int h);
    void upload(const render::Framebuffer& fb, const std::vector<core::Rect>& rects);
    void blit(HDC dst, const RECT& r) const;
    void release();

private:
    HDC memDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* bits_ = nullptr;
    int w_ = 0, h_ = 0;
};

struct ProcessSample {
    std::string label;
    unsigned long gdiObjects = 0;
    unsigned long userObjects = 0;
    unsigned long handleCount = 0;
    unsigned long long workingSetBytes = 0;
};

struct StatsLog {
    std::vector<update::RenderStats> frames;
    std::vector<ProcessSample> samples;
    long long qpcFrequency = 0;
    long long firstPresentQpc = 0;
    long long firstViewportMicros = -1;
};

class BenchScript; // bench_script.cpp
// Read-only HTML preview state (preview_mode.cpp).
struct PreviewState {
    html::PreviewDocument doc;
    layout::PreviewLayout layout;
    int scrollY = 0;
    std::size_t anchor = 0, caret = 0;
};

struct Options {
    std::optional<std::filesystem::path> file;
    std::optional<std::filesystem::path> fontsDir;
    std::optional<std::filesystem::path> statsJson;
    std::optional<std::filesystem::path> benchScript;
    bool exitAfterFirstPresent = false;
    bool noPreferences = false;
};

struct App {
    App();  // both defined in bench_script.cpp, where BenchScript is complete
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    HINSTANCE instance = nullptr;
    HWND main = nullptr;
    HWND toolbar = nullptr;
    HWND fontCombo = nullptr;
    HWND fontLabel = nullptr;
    HWND fontTip = nullptr;
    HFONT uiFont = nullptr;
    std::vector<std::unique_ptr<std::string>> comboIds;
    HWND status = nullptr;
    HWND surface = nullptr;
    HACCEL accel = nullptr;
    Options options;

    core::DiagnosticsLog log;
    std::unique_ptr<fonts::FontRegistry> registry;
    std::unique_ptr<fonts::RasterCache> cache;
    document::TextDocument doc;
    std::unique_ptr<layout::LayoutEngine> layout;
    layout::Viewport viewport;
    std::unique_ptr<update::UpdateManager> updates;
    Win32Clipboard clipboard{&main};
    std::unique_ptr<editor::EditorController> editor;
    storage::Win32FileSystem fileSystem;
    PromptService prompts{&main};
    FileDialogs dialogs{&main};
    std::unique_ptr<editor::FileWorkflow> workflow;
    std::unique_ptr<storage::IPreferencesStore> prefs;

    render::Framebuffer fb;
    GdiPresenter presenter;
    int scale = 1;
    bool caretOn = true;
    bool mouseSelecting = false;
    bool firstPresentDone = false;
    bool overlayOn = false;
    bool benchMode = false;
    long long pendingInputQpc = 0; // QPC of the oldest unpresented input (keystroke-to-present)
    long long openDecodedQpc = 0;  // QPC when a file finished decoding (first-viewport timing)
    StatsLog stats;
    update::RenderStats lastFrame;        // most recent presented frame (F12 overlay)
    long long lastKeyToPresentMicros = -1; // most recent keystroke-to-present
    std::unique_ptr<BenchScript> bench;
    std::unique_ptr<PreviewState> preview; // non-null while an HTML file is shown read-only
    int lastPreviewScrollY = 0;            // preview scroll position restored after View -> Page Source
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    // Shell operations (implemented across main_window.cpp, pixel_surface.cpp, file_commands.cpp).
    void initEngine();
    void afterChange();     // refreshes title, status, scrollbars and schedules a repaint
    void requestPaint();    // invalidates the surface area covered by the dirty region
    void updateTitle();
    void updateStatus();
    void updateScrollbars();
    void layoutChildren();
    void resetCaretBlink();
    bool idle();            // background work; returns true if more work is pending
    void command(int id);   // menu / accelerator / toolbar commands
    void emergencySave() noexcept;
    void savePreferences();
    void recordSample(const char* label);
    void writeStatsJson();
    void onPresented(const update::RenderStats& frame, bool painted); // after BitBlt (pixel_surface.cpp)
    void drawOverlay(render::Framebuffer& fb, std::vector<core::Rect>& presented, const update::RenderStats& current);
    core::Rect overlayRect() const;
    void setFont(const std::string& packId);
};

App& app();

int scaleForDpi(UINT dpi) noexcept; // research R-08 integer scale policy

// Window classes / creation (main_window.cpp, pixel_surface.cpp, toolbar.cpp).
bool registerWindowClasses(HINSTANCE inst);
bool registerSurfaceClass(HINSTANCE inst);
HWND createMainWindow(HINSTANCE inst);
HWND createSurface(HWND parent);
HWND createToolbar(HWND parent);
void syncToolbar();
void populateFontCombo();
void showAboutFonts();
void showDiagnostics();
void showAbout();
std::wstring aboutText();
long long qpcNow() noexcept;
void handleKeyDown(WPARAM vk, bool shift, bool ctrl); // pixel_surface.cpp; also used by the bench runner
void scrollRows(int rows);
void startBenchScript(const std::filesystem::path& script);
void benchStep(); // runs the next bench-script command (bench_script.cpp)
bool isHtmlPath(const std::filesystem::path& p);
bool openPreview(const std::filesystem::path& path, const std::u32string& text);
void closePreview();
bool canToggleSource();  // an HTML file is open (preview or source view)
bool sourceViewShown();  // the HTML source is shown in the editor
void toggleSource();     // View -> Page Source (Ctrl+U)
void rebuildPreview();
void paintPreviewFrame(std::vector<core::Rect>& rects);
void previewScroll(int dy);
bool previewKey(WPARAM vk, bool ctrl);
void previewMouse(int x, int y, bool down, bool drag, bool extend);
bool previewCommand(int id);
void previewScrollbars();
std::wstring previewTitle();
constexpr UINT WM_APP_BENCH_NEXT = WM_APP + 1;
constexpr UINT WM_APP_BENCH_KEY = WM_APP + 2;
void showInfoTable(const std::wstring& title, const std::vector<std::wstring>& columns,
                   const std::vector<std::vector<std::wstring>>& rows, const std::wstring& footer);

// File commands (file_commands.cpp).
void fileNew();
void fileOpen(std::optional<std::filesystem::path> path = std::nullopt);
void fileSave();
void fileSaveAs();
bool fileRequestExit();

} // namespace pxc::shell
