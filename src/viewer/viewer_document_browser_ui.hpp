#pragma once

#include "viewer_document_browser.hpp"

struct ImGuiMultiSelectIO;

namespace elf3d::viewer {
// Immediate UI call context; no borrow escapes the current build invocation.
struct BrowserPanelContext {
    ViewerFrameContext& frame;
    SceneSession& scene;
    Viewport& viewport;
    ToolCoordinator& tools;
    ViewerCommandDispatcher& commands;
};
void draw_browser_row(BrowserPanelContext& context, BrowserProjection& projection,
                      std::size_t row_index, float height);
void draw_browser_rows(BrowserPanelContext& context, BrowserProjection& projection, bool resources);
void apply_browser_selection_requests(BrowserPanelContext& context,
                                      const BrowserProjection& projection,
                                      const ImGuiMultiSelectIO& io, bool resources);
[[nodiscard]] const char* browser_text_end(std::string_view text) noexcept;
void browser_text(std::string_view text);
void draw_resource_context(BrowserPanelContext& context, const BrowserRow& row);
void draw_resource_visibility(BrowserPanelContext& context, const BrowserRow& row,
                              std::size_t row_index);
} // namespace elf3d::viewer
