// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "core/Application.h"
#include "editor/EditorDraw.h"
#include "imgui/imgui.h"

namespace Editor {

class EditorApp;
class UndoSystem;

struct UIObject;
struct UIStyle;

class UIView;

class MFDDetailsPane;

class MFDEditor : public Application::Lifecycle {
public:
	MFDEditor(EditorApp *app);
	~MFDEditor();

	UndoSystem *GetUndo();

	UIView *GetRootView();

	// convert a coordinate in view-space to a coordinate in editor screen space (for drawlists)
	ImVec2 ViewToScreen(const ImVec2 &pos);

	void SetSelectedObject(UIObject *obj);
	UIObject *GetSelectedObject() const;

	// return the bottom-most UIObject the mouse is currently hovering
	UIObject *GetHoveredObject();

	// Create and return a new UIObject with a valid ID and Style assigned
	UIObject *CreateNewObject();

	// Create and return a new UIStyle with a valid initial font assigned
	UIStyle *CreateNewStyle();

	// Return the default style used by the editor
	UIStyle *GetDefaultStyle();

protected:
	void Start() override;

	void Update(float deltaTime) override;

	void End() override;

private:

	void DrawInterface();

	void DrawOutlinePanel();

	bool DrawOutlineEntry(UIObject *obj);

	void DrawToolbar();

	void DrawLayoutView(ImRect area);

	void DrawPreview(ImDrawList *outputDl);

	void DrawObjectHighlight(ImDrawList *outputDl, UIObject *obj, ImU32 col);

	void DrawUndoStack();

	void DrawDebugWindow();

	void HandleViewportInteraction(bool clicked, bool wasPressed);

private:
	EditorApp *m_app;
	std::unique_ptr<UndoSystem> m_undoSystem;

	std::unique_ptr<MFDDetailsPane> m_detailsPane;

	// Position of the viewport origin in screen coordinates
	ImVec2 m_viewportScreenPos;

	// Position of the mouse relative to the viewport origin
	ImVec2 m_viewportMousePos;

	// Negative scroll in viewport, thus scroll of (100, 100) moves the origin
	// of the grid 100 pixels into the visible viewport space.
	// Viewport scroll is stored in real screen pixels (post-zoom space)
	ImVec2 m_viewportScroll;

	// Positive viewport zoom; 2.0 zoom makes objects twice as big.
	// When the viewport is zoomed, the scroll value is updated to the new
	// screen offset based on the zoom factor
	float m_viewportZoom;

	uint32_t m_lastId;

	UIObject *m_selectedObject;
	UIObject *m_nextObject;

	bool m_hasNextObject;

	UIObject *m_rootObject;

	std::unique_ptr<UIView> m_rootView;
	UIStyle *m_defaultStyle;

	bool m_viewportHovered;
	bool m_viewportActive;

	bool m_metricsWindow;
	bool m_undoWindow;
	bool m_debugWindow;
};

} // namespace Editor
