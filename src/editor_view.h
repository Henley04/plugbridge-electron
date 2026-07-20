//-----------------------------------------------------------------------------
// evst3 — Electron VST3 Audio Plugin Bridge
// EditorView — owns the IPlugView returned by a plugin's IEditController and
// drives its lifecycle (create → attach → resize → detach → destroy) against
// a native parent window handle supplied by the Electron BrowserWindow.
//
// The host side of the editor contract is implemented here:
//   - IPlugFrame (Steinberg::IPlugFrame) — the interface the plugin's view
//     calls back into when it wants to resize itself or be re-parented. We
//     route resize requests through a callback to PluginInstance, which
//     emits a JS 'editorResize' event so the Electron renderer can update
//     its layout / BrowserWindow bounds.
//   - IPlugViewContentScaleSupport (Steinberg::IPlugViewContentScaleSupport)
//     — optional interface on the *plugin's* IPlugView that lets the host
//     push a HiDPI / retina scale factor. We query it lazily after
//     IPlugView::isPlatformTypeSupported returns kResultTrue and forward
//     setContentScaleFactor calls from JS.
//
// Platform-specific notes:
//   - Windows:  parent handle is an HWND (passed as a BigInt / Number from
//               BrowserWindow.getNativeWindowHandle()).
//   - macOS:    parent handle is an NSView* (the contentView of an
//               NSWindow; BrowserWindow.getNativeWindowHandle() returns
//               the NSView* on macOS in Electron).
//   - Linux:    parent handle is an X11 Window id (the numeric id of the
//               X11 window Electron creates for the BrowserWindow). The
//               id is passed directly to IPlugView::attached under the
//               "X11EmbedWindowID" platform type — no GTK linkage is
//               required.
//
// The plugin-side platform type string we negotiate with via
// IPlugView::isPlatformTypeSupported is:
//   - Windows:  kPlatformTypeHWND ("HWND")
//   - macOS:    kPlatformTypeNSView ("NSView")
//   - Linux:    kPlatformTypeX11EmbedWindowID ("X11EmbedWindowID")
//-----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"  // Steinberg::IPlugViewContentScaleSupport
#include "pluginterfaces/vst/ivsteditcontroller.h"           // Steinberg::Vst::IEditController

namespace evst3 {

// Forward declarations
class PluginInstance;

// ViewRect — straight copy of Steinberg::ViewRect, exposed to JS via
// plugin.getEditorSize(). Mirrors (left, top, right, bottom) in pixels.
struct EditorViewRect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
    int32_t width() const { return right - left; }
    int32_t height() const { return bottom - top; }
};

// Callback the EditorView invokes when the plugin requests a new size via
// IPlugFrame::resizeView. The PluginInstance routes this to the JS
// 'editorResize' event so the host can update its layout. Returns true if
// the host accepted the new size (in which case the EditorView calls
// IPlugView::onSize); false if the host rejected it (no onSize call).
using EditorResizeCallback = std::function<bool(const EditorViewRect&)>;

// EditorView owns one IPlugView instance and implements IPlugFrame so the
// plugin's view can call back into the host for resize / re-parent requests.
//
// Lifecycle:
//   1. hasEditor() — probe IEditController::createView("editor") without
//      retaining; returns true if a non-null IPlugView was returned.
//   2. openEditor(parentHandle) — call createView, query platform support,
//      call attached(parent, platformType), set the resize callback. The
//      IPlugView refcount is held for the lifetime of the EditorView.
//   3. getEditorSize() — read ViewRect from IPlugView::getSize.
//   4. setEditorScale(factor) — forward to IPlugViewContentScaleSupport
//      when the plugin implements it; no-op otherwise (returns false).
//   5. closeEditor() — call IPlugView::removed(), release the IPlugView.
class EditorView final
    : public Steinberg::U::Implements<Steinberg::U::Directly<Steinberg::IPlugFrame>> {
public:
    EditorView();
    ~EditorView() noexcept override;

    // Bind this EditorView to a PluginInstance (used so the IPlugFrame
    // callback can dispatch JS events through the instance). Set to nullptr
    // when the PluginInstance is torn down.
    void setPluginInstance(PluginInstance* instance) { instance_ = instance; }

    // Set the callback invoked when the plugin requests a new editor size
    // via IPlugFrame::resizeView. The callback is invoked synchronously
    // from the controller thread.
    void setResizeCallback(EditorResizeCallback cb) { resizeCb_ = std::move(cb); }

    //--- Editor lifecycle (called by PluginInstance) -------------------

    // Create the IPlugView from the supplied IEditController and attach it
    // to the supplied native parent handle. The handle's interpretation
    // depends on the platform (see file header). Returns true on success.
    // On failure, the IPlugView (if any) is released and the EditorView
    // returns to the closed state.
    //
    // `controller` is the plugin's IEditController (must outlive the
    // EditorView). `parentHandle` is the raw pointer / window id cast to
    // uintptr_t (JS callers pass a BigInt or Number).
    bool openEditor(Steinberg::Vst::IEditController* controller,
                    uintptr_t parentHandle);

    // Detach the editor from its parent and release the IPlugView. Safe to
    // call when no editor is open (no-op). Idempotent.
    void closeEditor();

    // Returns true if an IPlugView is currently attached.
    bool isOpen() const { return plugView_ != nullptr; }

    // Read the current editor size from the IPlugView. Returns {0,0,0,0}
    // if no editor is open.
    EditorViewRect getEditorSize() const;

    // Forward a content-scale factor (e.g. 2.0 for retina) to the plugin's
    // IPlugViewContentScaleSupport. Returns true if the plugin implements
    // the interface and accepted the value; false otherwise.
    bool setEditorScale(float factor);

    //--- IPlugFrame ----------------------------------------------------
    // Called by the plugin's IPlugView when it wants to resize itself.
    // We forward the request to the resizeCb_ (set by PluginInstance),
    // which dispatches a JS 'editorResize' event. If the host accepts
    // the new size, we call plugView_->onSize(&newRect) to inform the
    // plugin that the resize has been applied.
    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect* newRect) override;

private:
    PluginInstance* instance_ = nullptr;
    Steinberg::IPtr<Steinberg::IPlugView> plugView_;
    Steinberg::IPtr<Steinberg::IPlugViewContentScaleSupport> scaleSupport_;
    EditorResizeCallback resizeCb_;
    uintptr_t parentHandle_ = 0;
    bool attached_ = false;
};

// Returns the platform-type string that IPlugView::isPlatformTypeSupported
// expects on the current OS. Used by EditorView::openEditor to negotiate
// the parent-handle kind before calling IPlugView::attached.
const char* currentPlatformTypeString();

} // namespace evst3
