// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "core/Application.h"
#include "editor/EditorDraw.h"
#include "imgui/imgui.h"

namespace Editor {

class MFDEditor;
class UIObject;
class UIStyle;

// TODO: inherit from some sort of EditorWindow abstraction
class MFDDetailsPane {
public:
	MFDDetailsPane(MFDEditor *editor);
	~MFDDetailsPane();

	void SetLayoutArea(const ImRect &layoutArea);

	void Draw();

	void OpenStyleEditor(UIStyle *style);

	void CloseStyleEditor();

private:

	void DrawEditorDetails();

	void DrawObjectDetails(UIObject *object);

	void DrawStyleEditor(UIStyle *style);

	// Count the total number of objects in this view using the given style
	size_t CountStyleUsers(UIStyle *style);

private:
	MFDEditor *m_editor;
	ImRect m_layoutArea;

	UIStyle *m_currentStyle;
	std::string m_newStyleName;
	bool m_styleEditor;
	bool m_newStyle;
	bool m_renameStyle;
};

} // namespace Editor
