// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "MFDDetailsPane.h"

#include "MFDEditor.h"
#include "MFDEditorHelpers.h"
#include "MFDEditorUndo.h"

#include "UIObject.h"
#include "UIView.h"

#include "editor/EditorDraw.h"
#include "editor/UndoSystem.h"
#include "editor/UndoStepType.h"
#include "fmt/core.h"
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

using namespace Editor;

namespace {

	static constexpr const char *AxisModes[] = {
		"Horizontal",
		"Vertical"
	};

	static constexpr const char *SizeModes[] = {
		"Size to Content",
		"Fixed Size",
		"% of Parent Size",
		"Size to Children",
	};

	static constexpr const char *ExpandModes[] = {
		"Align Start",
		"Align Center",
		"Align End",
		"Fill",
		"Keep Size",
	};

	static constexpr const char *AlignModes[] = {
		"Align Start",
		"Align Center",
		"Align End",
	};

	static constexpr const char *ContentTypes[] = {
		"None",
		"Text",
	};

}

MFDDetailsPane::MFDDetailsPane(MFDEditor *editor) :
	m_editor(editor),
	m_currentStyle(nullptr),
	m_styleEditor(false),
	m_newStyle(false),
	m_renameStyle(false)
{
}

MFDDetailsPane::~MFDDetailsPane()
{
}

void MFDDetailsPane::SetLayoutArea(const ImRect &layoutArea)
{
	m_layoutArea = layoutArea;
}

void MFDDetailsPane::Draw()
{
	UIView *rootView = m_editor->GetRootView();
	UIObject *selectedObject = m_editor->GetSelectedObject();

	// TODO: tabbed layout containing object details, styles, and vars
	std::string label = "Details";
	if (m_styleEditor)
		label = fmt::format("Style Details: {}", rootView->GetStyleName(m_currentStyle));
	if (selectedObject)
		label = fmt::format("Object Details: {}", selectedObject->label.sv());

	Draw::BeginWindow(m_layoutArea, fmt::format("{}###Details", label).c_str());

	if (m_styleEditor) {
		DrawStyleEditor(m_currentStyle);
	} else if (selectedObject) {
		DrawObjectDetails(selectedObject);
	} else {
		DrawEditorDetails();
	}

	if (!m_styleEditor) {
		// defer clearing the current style ptr until the end of the frame
		m_currentStyle = nullptr;
	}

	ImGui::End();
}

void MFDDetailsPane::OpenStyleEditor(UIStyle *style)
{
	m_styleEditor = true;
	m_currentStyle = style;

	m_newStyle = false;
	m_renameStyle = false;
}

void MFDDetailsPane::CloseStyleEditor()
{
	m_styleEditor = false;
}

void MFDDetailsPane::DrawEditorDetails()
{
	PROFILE_SCOPED()

	UndoSystem *undo = m_editor->GetUndo();
	UIView *rootView = m_editor->GetRootView();

	ImVec2 val = rootView->GetViewSize();
	if (Draw::EditFloat2("View Size", &val))
		rootView->SetViewSize(val);

	if (Draw::UndoHelper("Edit View Size", undo))
		AddUndoGetSetValue<&UIView::GetViewSize, &UIView::SetViewSize>(undo, rootView);

	UIObject *hovered = m_editor->GetHoveredObject();
	ImGui::Text("Hovered: %d", hovered ? hovered->id : -1);

	// Draw selection dropdown to pick style to edit
	UIView::StyleContainer &styles = m_editor->GetRootView()->GetStyles();
	if (ImGui::BeginCombo("##Styles", "Edit Style")) {
		size_t idx = 0;

		for (auto &pair : styles) {
			std::string label = fmt::format("{}##{}", pair.first, idx++);

			if (ImGui::Selectable(label.c_str())) {
				m_currentStyle = pair.second.get();
				m_styleEditor = true;
			}

			idx++;
		}

		ImGui::EndCombo();
	}

	if (m_newStyle) {
		if (ImGui::Button("Cancel"))
			m_newStyle = false;

		ImGui::SameLine();

		if (ImGui::Button("Create")) {
			m_newStyle = false;
			m_styleEditor = true;
			m_currentStyle = m_editor->CreateNewStyle();

			undo->BeginEntry("Create Style");
			undo->AddUndoStep<UndoAddRemoveStyle>(rootView, m_newStyleName, m_currentStyle);
			undo->EndEntry();
		}

		ImGui::InputText("Style Name", &m_newStyleName);

	} else if (ImGui::Button("New Style")) {
		m_newStyle = true;
		m_newStyleName.clear();
	}
}

void MFDDetailsPane::DrawObjectDetails(UIObject *obj)
{
	PROFILE_SCOPED()

	UIView *rootView = m_editor->GetRootView();
	UndoSystem *undo = m_editor->GetUndo();

	// Object Label
	// ==========================================

	std::string label = std::string(obj->label.sv());
	if (ImGui::InputText("Label", &label))
		obj->label = StringName(label);

	if (Draw::UndoHelper("Edit Label", undo))
		AddUndoSingleValue(undo, &obj->label);

	// Object Features
	// ==========================================

	uint32_t features = obj->features;
	bool changed = false;
	if (Draw::ComboUndoHelper("Edit Features", "Features", undo)) {
		if (ImGui::IsWindowAppearing())
			AddUndoSingleValue(undo, &obj->features);

		changed |= ImGui::CheckboxFlags("Draw Border", &features, UIFeature_DrawBorder);
		changed |= ImGui::CheckboxFlags("Draw Background", &features, UIFeature_DrawBackground);
		changed |= ImGui::CheckboxFlags("Clickable", &features, UIFeature_Clickable);
		changed |= ImGui::CheckboxFlags("Scrollable", &features, UIFeature_Scrollable);
		changed |= ImGui::CheckboxFlags("Hover Animation", &features, UIFeature_HoverAnim);
		changed |= ImGui::CheckboxFlags("Active Animation", &features, UIFeature_ActiveAnim);
		changed |= ImGui::CheckboxFlags("Inherit Animations", &features, UIFeature_InheritAnim);
		changed |= ImGui::CheckboxFlags("Overlay Layout", &features, UIFeature_OverlayLayout);

		if (changed)
			obj->features = UIFeature(features);

		ImGui::EndCombo();
	}

	// Object Style
	// ==========================================

	// TODO: available styles should be managed by the editor
	std::string_view styleName = rootView->GetStyleName(obj->style);
	if (Draw::ComboUndoHelper("Edit Style", "Style", styleName.data(), undo)) {
		if (ImGui::IsWindowAppearing())
			AddUndoSingleValue(undo, &obj->style);

		for (auto &pair : rootView->GetStyles()) {
			if (ImGui::Selectable(pair.first.c_str(), pair.second.get() == obj->style))
				obj->style = pair.second.get();
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.f);

	if (ImGui::Button(">##EditStyle")) {
		OpenStyleEditor(obj->style);
	}

	// Object Layout Settings
	// ==========================================

	if (obj->parent) {

		ImGui::Separator();

		Draw::EditFloat2("Size:", &obj->size);
		if (Draw::UndoHelper("Edit Size", undo))
			AddUndoSingleValue(undo, &obj->size);

		if (Draw::LayoutHorizontal("Size Mode:", 2, ImGui::GetFontSize())) {
			Draw::EditOptions("Edit Size Mode X", "X", SizeModes, undo, &obj->sizeMode[0]);
			Draw::EditOptions("Edit Size Mode Y", "Y", SizeModes, undo, &obj->sizeMode[1]);

			Draw::EndLayout();
		}

		if (obj->parent->features & UIFeature_OverlayLayout) {

			if (Draw::LayoutHorizontal("Alignment Mode:", 2, ImGui::GetFontSize())) {
				Draw::EditOptions("Edit Alignment X", "X", AlignModes, undo, &obj->alignment[0]);
				Draw::EditOptions("Edit Alignment Y", "Y", AlignModes, undo, &obj->alignment[1]);

				Draw::EndLayout();
			}

		} else {

			if (Draw::LayoutHorizontal("Expansion Mode:", 2, ImGui::GetFontSize())) {
				Draw::EditOptions("Edit Expansion X", "X", ExpandModes, undo, &obj->alignment[0]);
				Draw::EditOptions("Edit Expansion Y", "Y", ExpandModes, undo, &obj->alignment[1]);

				Draw::EndLayout();
			}

		}

	}

	if (!(obj->features & UIFeature_OverlayLayout)) {
		Draw::EditOptions("Edit Primary Axis", "PrimaryAxis", AxisModes, undo, &obj->primaryAxis);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Object Contents
	// ==========================================

	Draw::EditOptionsButtons("Edit Content Type", "Content Type:", ContentTypes, undo, &obj->contentType);

	if (obj->contentType == ContentType_Text) {
		ImGui::InputText("Content", &obj->content);

		if (Draw::UndoHelper("Edit Content", undo))
			AddUndoSingleValue(undo, &obj->content);
	}

	// Object Content Alignment
	// ==========================================

	if (obj->contentType != ContentType_None) {
		if (Draw::LayoutHorizontal("Content Alignment:", 2, ImGui::GetFontSize())) {
			Draw::EditOptions("Edit Content Alignment X", "X", AlignModes, undo, &obj->contentAlign[0]);
			Draw::EditOptions("Edit Content Alignment Y", "Y", AlignModes, undo, &obj->contentAlign[1]);

			Draw::EndLayout();
		}
	}
}

void MFDDetailsPane::DrawStyleEditor(UIStyle *style)
{
	PROFILE_SCOPED()

	UndoSystem *undo = m_editor->GetUndo();
	UIView *rootView = m_editor->GetRootView();

	if (!m_renameStyle) {

		size_t numUsers = CountStyleUsers(style);

		if (ImGui::Button("Back")) {
			m_styleEditor = false;
		}

		ImGui::SameLine();

		if (numUsers == 0 && ImGui::Button("Delete Style")) {
			m_styleEditor = false;

			undo->BeginEntry("Delete Style");
			undo->AddUndoStep<UndoAddRemoveStyle>(rootView, rootView->GetStyleName(style));
			undo->EndEntry();
		}

		ImGui::SameLine();

		if (ImGui::Button("Rename Style")) {
			m_renameStyle = true;
			m_newStyleName = rootView->GetStyleName(style);
		}

		ImGui::SameLine();

		ImGui::Text("Users: %ld", numUsers);

	} else {

		if (ImGui::Button("Cancel")) {
			m_renameStyle = false;
			m_newStyleName.clear();
		}

		ImGui::SameLine();

		if (ImGui::Button("Save")) {
			m_renameStyle = false;

			undo->BeginEntry("Rename Style");
			undo->AddUndoStep<UndoAddRemoveStyle>(rootView, rootView->GetStyleName(style), m_newStyleName);
			undo->EndEntry();
		}

		ImGui::InputText("Style Name", &m_newStyleName);

	}

	std::string_view fontName = rootView->GetFontName(style->font);
	ImGui::Text("Font Name: %s", fontName.empty() ? "<unknown>" : fontName.data());

	ImGui::InputFloat("Font Size", &style->fontSize);
	if (Draw::UndoHelper("Edit Font Size", undo))
		AddUndoSingleValue(undo, &style->fontSize);

	ImGui::Separator();
	ImGui::Spacing();

	ImColor color = style->color;
	bool changed = ImGui::ColorEdit4("Content##Color", &color.Value.x);
	if (Draw::UndoHelper("Edit Color", undo))
		AddUndoSingleValue(undo, &style->color);

	if (changed)
		style->color = color;

	color = style->backgroundColor;
	changed = ImGui::ColorEdit4("Background##Color", &color.Value.x);
	if (Draw::UndoHelper("Edit Background Color", undo))
		AddUndoSingleValue(undo, &style->backgroundColor);

	if (changed)
		style->backgroundColor = color;

	Draw::EditFloat2("Padding", &style->padding);
	if (Draw::UndoHelper("Edit Padding", undo))
		AddUndoSingleValue(undo, &style->padding);

	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextUnformatted("Border:");
	ImGui::PushID("Border");

	ImGui::InputFloat("Rounding", &style->borderRounding);
	if (Draw::UndoHelper("Edit Border Rounding", undo))
		AddUndoSingleValue(undo, &style->borderRounding);

	ImGui::InputFloat("Thickness", &style->borderThickness);
	if (Draw::UndoHelper("Edit Border Thickness", undo))
		AddUndoSingleValue(undo, &style->borderThickness);

	color = style->borderColor;
	changed = ImGui::ColorEdit4("Color", &color.Value.x);
	if (Draw::UndoHelper("Edit Border Color", undo))
		AddUndoSingleValue(undo, &style->borderColor);

	if (changed)
		style->borderColor = color;

	ImGui::PopID();
}

size_t MFDDetailsPane::CountStyleUsers(UIStyle *style)
{
	PROFILE_SCOPED()

	size_t users = 0;
	UIObject *root = m_editor->GetRootView()->GetRoot();

	if (style == m_editor->GetDefaultStyle())
		users++;

	if (style == root->style)
		users++;

	std::vector<std::pair<const UIObject *, size_t>> objStack;
	objStack.reserve(32);

	if (!root->children.empty())
		objStack.push_back({ root, 0 });

	while (!objStack.empty()) {
		auto &pair = objStack.back();

		const UIObject *obj = pair.first->children[pair.second++].get();
		if (pair.second == pair.first->children.size())
			objStack.pop_back();

		if (obj->style == style)
			users++;

		if (!obj->children.empty())
			objStack.push_back({ obj, 0 });
	}

	return users;
}
