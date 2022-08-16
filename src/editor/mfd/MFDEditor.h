// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "core/Application.h"
#include "editor/EditorDraw.h"
#include "imgui/imgui.h"

namespace Editor {

class EditorApp;

struct UIObject;
struct UIStyle;

class UIView;

class MFDEditor : public Application::Lifecycle {
public:
	MFDEditor(EditorApp *app);
	~MFDEditor();

protected:
	void Start() override;

	void Update(float deltaTime) override;

	void End() override;

private:

	void DrawInterface();

	void DrawDetailsPanel();

	void DrawObjectDetails();

	void DrawOutlinePanel();

	bool DrawOutlineEntry(UIObject *obj);

	void DrawLayoutView(ImRect area);

	void DrawPreview(ImDrawList *outputDl);

	void UpdateLayout(ImRect layoutArea);

	void HandleViewportInteraction(bool clicked, bool wasPressed);

	void SetSelectedObject(UIObject *obj);

	// return the currently hovered UI object, ignoring widget feature flags
	UIObject *GetHoveredObject();

private:
	EditorApp *m_app;

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
	UIObject *m_rootObject;

	std::unique_ptr<UIView> m_rootView;
	UIStyle *m_defaultStyle;

	bool m_metricsWindow;
	bool m_viewportHovered;
	bool m_viewportActive;
};

} // namespace Editor
