//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// EditorView implementation — drives IPlugView lifecycle and forwards
// IPlugFrame::resizeView requests back to JS via PluginInstance.
//-----------------------------------------------------------------------------
#include "editor_view.h"

#include "errors.h"
#include "plugin_instance.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <AppKit/AppKit.h>
#elif defined(__linux__)
#  include <gdk/gdk.h>
#  include <gtk/gtk.h>
#endif

namespace evst3 {

//------------------------------------------------------------------------
// Platform-type string — the IPlugView contract requires the host to pass
// a platform-specific string to isPlatformTypeSupported / attached. We
// expose the correct constant per OS so the EditorView can negotiate.
//------------------------------------------------------------------------
const char* currentPlatformTypeString() {
#if defined(_WIN32)
    return Steinberg::kHWND;          // "HWND"
#elif defined(__APPLE__)
    return Steinberg::kNSView;        // "NSView"
#elif defined(__linux__)
    return Steinberg::kX11EmbedWindowID; // "X11EmbedWindowID"
#else
#  error "Unsupported platform for EditorView"
#endif
}

//------------------------------------------------------------------------
// EditorView ctor / dtor
//------------------------------------------------------------------------
EditorView::EditorView() = default;

EditorView::~EditorView() noexcept {
    // Ensure the editor is detached before the IPlugView is released.
    // IPlugView::removed() must be called before the last release() so
    // the plugin can tear down its platform-side resources while it still
    // has access to the parent handle.
    closeEditor();
    instance_ = nullptr;
}

//------------------------------------------------------------------------
// openEditor — create the IPlugView, validate platform support, attach.
//------------------------------------------------------------------------
bool EditorView::openEditor(Steinberg::Vst::IEditController* controller,
                            uintptr_t parentHandle) {
    if (!controller) {
        return false;
    }
    if (plugView_) {
        // Editor already open. The caller must closeEditor() first.
        return false;
    }
    if (parentHandle == 0) {
        return false;
    }

    // Create the editor view. The "editor" string is the canonical name
    // defined by the VST3 SDK for the default editor.
    Steinberg::IPlugView* view = controller->createView("editor");
    if (!view) {
        return false;
    }

    // Probe optional IPlugViewContentScaleSupport so JS can later call
    // setEditorScale() without re-querying.
    scaleSupport_ = Steinberg::U::cast<Steinberg::Vst::IPlugViewContentScaleSupport>(view);

    // Negotiate the platform type. The plugin may return kResultTrue
    // (accepted), kResultFalse (rejected — we cannot attach), or
    // kNotImplemented. Only proceed when accepted.
    const char* platformType = currentPlatformTypeString();
    if (view->isPlatformTypeSupported(platformType) != Steinberg::kResultTrue) {
        // Plugin does not support the host's native window type. Release
        // the view and bail — no editor can be shown.
        view->release();
        return false;
    }

    // Take ownership of the view via IPtr. From here on, the EditorView
    // is responsible for releasing it (closeEditor or dtor).
    plugView_ = Steinberg::owned(view);
    parentHandle_ = parentHandle;

    // Wire ourselves as the IPlugFrame so the plugin can call
    // resizeView() when it wants to resize the editor.
    if (view->setFrame(this) != Steinberg::kResultTrue) {
        // setFrame rejection is non-fatal: the plugin just won't be able
        // to request resizes. Continue with attach.
    }

    // Attach to the parent. The parent handle is interpreted as an HWND
    // (Windows), NSView* (macOS), or Window id (Linux) per the platform
    // type string we negotiated above.
    if (view->attached(reinterpret_cast<void*>(parentHandle), platformType)
        != Steinberg::kResultTrue) {
        // Attach failed. Release the view and bail.
        plugView_.reset();
        scaleSupport_.reset();
        parentHandle_ = 0;
        return false;
    }
    attached_ = true;
    return true;
}

//------------------------------------------------------------------------
// closeEditor — detach and release the IPlugView.
//------------------------------------------------------------------------
void EditorView::closeEditor() {
    if (!plugView_) {
        return;
    }
    if (attached_) {
        // removed() must be called before the last release() so the plugin
        // can tear down platform-side resources while it still has the
        // parent handle.
        plugView_->removed();
        attached_ = false;
    }
    plugView_.reset();
    scaleSupport_.reset();
    parentHandle_ = 0;
}

//------------------------------------------------------------------------
// getEditorSize — read the ViewRect from the IPlugView.
//------------------------------------------------------------------------
EditorViewRect EditorView::getEditorSize() const {
    EditorViewRect r{};
    if (!plugView_) {
        return r;
    }
    Steinberg::ViewRect vr;
    if (plugView_->getSize(&vr) == Steinberg::kResultTrue) {
        r.left = vr.left;
        r.top = vr.top;
        r.right = vr.right;
        r.bottom = vr.bottom;
    }
    return r;
}

//------------------------------------------------------------------------
// setEditorScale — forward to IPlugViewContentScaleSupport.
//------------------------------------------------------------------------
bool EditorView::setEditorScale(float factor) {
    if (!scaleSupport_) {
        return false;
    }
    return scaleSupport_->setContentScaleFactor(factor) == Steinberg::kResultTrue;
}

//------------------------------------------------------------------------
// IPlugFrame::resizeView — plugin requests a new editor size.
//
// We forward the request to the resize callback (set by PluginInstance),
// which dispatches a JS 'editorResize' event. If the host accepts the new
// size (returns true), we call plugView_->onSize(&newRect) to inform the
// plugin that the resize has been applied. Otherwise we return kResultFalse
// and the plugin retains its current size.
//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API EditorView::resizeView(Steinberg::IPlugView* view,
                                                     Steinberg::ViewRect* newRect) {
    if (!view || !newRect) {
        return Steinberg::kInvalidArgument;
    }
    // Only honor resize requests from our own view.
    if (view != plugView_.get()) {
        return Steinberg::kResultFalse;
    }
    EditorViewRect r;
    r.left = newRect->left;
    r.top = newRect->top;
    r.right = newRect->right;
    r.bottom = newRect->bottom;

    bool accepted = false;
    if (resizeCb_) {
        accepted = resizeCb_(r);
    }
    if (!accepted) {
        return Steinberg::kResultFalse;
    }
    // Inform the plugin that the new size has been applied. Without this
    // call the plugin may keep requesting the same size indefinitely.
    if (plugView_->onSize(newRect) != Steinberg::kResultTrue) {
        // onSize rejection is non-fatal — the resize already happened on
        // the host side; the plugin will see the new size on its next
        // getSize() call.
    }
    return Steinberg::kResultTrue;
}

} // namespace evst3
