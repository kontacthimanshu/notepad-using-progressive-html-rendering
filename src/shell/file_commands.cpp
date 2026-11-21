// File menu commands: New, Open, Save, Save As, Exit — routed through editor::FileWorkflow, which owns
// the unsaved-changes prompt and never loses content on failure (FR-027, FR-034).
#include "app.hpp"

namespace pxc::shell {

namespace {
void refreshAfterFileChange() {
    auto& a = app();
    a.viewport.clampScroll(*a.layout);
    a.updates->invalidateAll();
    a.afterChange();
}
} // namespace

void fileNew() {
    if (app().workflow->newDocument()) {
        closePreview();
        refreshAfterFileChange();
    }
}

void fileOpen(std::optional<std::filesystem::path> path) {
    // First-viewport timing (SC-010) starts when decoding is complete; only the visible lines are laid out
    // before the first present, the rest is measured in idle slices (research R-12).
    const bool wasPreview = app().preview != nullptr;
    if (app().workflow->open(std::move(path))) {
        if (!app().preview && wasPreview) closePreview();
        app().openDecodedQpc = qpcNow();
        refreshAfterFileChange();
    }
}

void fileSave() {
    app().workflow->save();
    app().updateTitle();
    app().updateStatus();
}

void fileSaveAs() {
    app().workflow->saveAs();
    app().updateTitle();
    app().updateStatus();
}

bool fileRequestExit() { return app().workflow->requestExit(); }

} // namespace pxc::shell
