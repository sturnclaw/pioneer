// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "MFDEditor.h"

#include "MFDDetailsPane.h"
#include "MFDEditorHelpers.h"
#include "MFDEditorUndo.h"
#include "MFDIOManager.h"

#include "UIObject.h"
#include "UIView.h"

#include "editor/EditorApp.h"
#include "editor/EditorDraw.h"
#include "editor/UndoSystem.h"
#include "editor/UndoStepType.h"
#include "graphics/Graphics.h"
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

using namespace Editor;

// Copied from imgui_demo.cpp
namespace ImGui { void ShowFontAtlas(ImFontAtlas* atlas); }

constexpr float GRID_SPACING = 10.f;

MFDEditor::MFDEditor(EditorApp *app) :
	Application::Lifecycle(),
	m_app(app),
	m_viewportMousePos(-FLT_MAX, -FLT_MAX),
	m_viewportScroll(100.f, 100.f),
	m_viewportZoom(1.f),
	m_selectedObject(nullptr),
	m_nextObject(nullptr),
	m_hasNextObject(false),
	m_rootObject(nullptr),
	m_viewportHovered(false),
	m_viewportActive(false),
	m_metricsWindow(false),
	m_undoWindow(false),
	m_debugWindow(false)
{
	m_undoSystem.reset(new UndoSystem());
	m_detailsPane.reset(new MFDDetailsPane(this));
	m_ioManager.reset(new MFDIOManager(this));
}

MFDEditor::~MFDEditor()
{
}

UndoSystem *MFDEditor::GetUndo()
{
	return m_undoSystem.get();
}

UIView *MFDEditor::GetRootView()
{
	return m_rootView.get();
}

void MFDEditor::SetSelectedObject(UIObject *obj)
{
	// Defer object selection until end of the frame to avoid undo issues
	m_nextObject = obj;
	m_hasNextObject = true;

	if (obj)
		m_detailsPane->CloseStyleEditor();
}

UIObject *MFDEditor::GetSelectedObject() const
{
	return m_selectedObject;
}

UIObject *MFDEditor::GetHoveredObject()
{
	// convert viewport position into View coordinates
	ImVec2 viewPos = (m_viewportMousePos - m_viewportScroll) / m_viewportZoom;

	return m_rootView->GetObjectAtPoint(viewPos);
}

ImVec2 MFDEditor::ViewToScreen(const ImVec2 &vec)
{
	return vec * m_viewportZoom + m_viewportScroll + m_viewportScreenPos;
}

UIObject *MFDEditor::CreateNewObject()
{
	UIObject *child = new UIObject();
	child->Setup(m_lastId++, {}, m_defaultStyle);
	return child;
}

UIStyle *MFDEditor::CreateNewStyle()
{
	UIStyle *style = new UIStyle();
	style->fontSize = 16;
	style->font = m_rootView->GetOrLoadFont("pionillium", 16);
	return style;
}

UIStyle *MFDEditor::GetDefaultStyle()
{
	return m_defaultStyle;
}

void MFDEditor::SetEditedStyles(std::string_view path)
{
	m_styleFilepath = path;
}

std::string_view MFDEditor::GetStylePath()
{
	return m_styleFilepath;
}

std::string_view MFDEditor::GetLayoutPath()
{
	return m_layoutFilepath;
}

// ============================================================================
//  Lifecycle functions
// ============================================================================

void MFDEditor::Start()
{
	m_rootView.reset(new UIView(m_app->GetRenderer()));

	// Register the default font files
	m_rootView->RegisterFontFile("pionillium", "PionilliumText22L-Medium.ttf");

	Reset();
}

void MFDEditor::Reset()
{
	m_lastId = 0;

	// Clear any leftover state
	m_undoSystem->Clear();
	m_rootView->SetRoot(nullptr);
	m_rootView->GetStyles().clear();

	// Setup the "editor default" style
	m_defaultStyle = CreateNewStyle();
	m_defaultStyle->borderThickness = 2.f;

	// Register it in the style table
	m_rootView->GetStyles()["default"].reset(m_defaultStyle);

	// Setup a default size for this view
	m_rootView->SetViewSize(ImVec2(800, 600));

	// Setup the root object for this view
	m_rootObject = CreateNewObject();
	m_rootObject->features = UIFeature_DrawBorder;
	m_rootObject->label = "root"_name;

	m_rootView->SetRoot(m_rootObject);

	m_selectedObject = m_rootObject;
}

void MFDEditor::Update(float deltaTime)
{
	ImGuiID editorID = ImGui::GetID("MFDEditor");

	// Note: this is janky as heck, should try to ensure Input works correctly even while ImGui has keyboard focus
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, editorID, ImGuiInputFlags_RouteGlobal)) {
		m_undoSystem->Redo();
	} else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, editorID, ImGuiInputFlags_RouteGlobal)) {
		m_undoSystem->Undo();
	}

	m_rootView->Update(deltaTime);

	DrawInterface();

	if (m_hasNextObject) {
		m_selectedObject = m_nextObject;
		m_hasNextObject = false;
	}
}

void MFDEditor::End()
{
	m_lastId = 0;
	m_rootObject = nullptr;
	m_defaultStyle = nullptr;
	m_rootView.reset();
}

// ============================================================================
//  Interface drawing functions
// ============================================================================

void MFDEditor::DrawInterface()
{
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("File")) {
		ImGui::InputText("Filename", &m_layoutFilepath);

		if (ImGui::Button("Load")) {
			Reset();

			m_rootObject = m_ioManager->LoadLayout(m_layoutFilepath);

			m_rootView->SetRoot(m_rootObject);
			m_selectedObject = m_rootObject;
		}

		if (ImGui::Button("Save")) {
			m_ioManager->SaveLayout(m_layoutFilepath, m_styleFilepath);
		}

		if (ImGui::Button("Save Styles")) {
			m_ioManager->SaveStyles(m_styleFilepath);
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Tools")) {
		if (m_selectedObject && ImGui::Button("Add Child")) {
			UIObject *child = new UIObject();
			child->Setup(m_lastId++, {}, m_defaultStyle);

			GetUndo()->BeginEntry("Add Child");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(m_selectedObject, child);
			GetUndo()->EndEntry();
		}

		ImGui::Checkbox("Metrics Window", &m_metricsWindow);
		ImGui::Checkbox("Undo Stack", &m_undoWindow);
		ImGui::Checkbox("Debug Window", &m_debugWindow);
		ImGui::EndMenu();
	}
	// TODO: add main menu contents here
	ImGui::EndMainMenuBar();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);

	// Calculate layout sizing here
	ImVec2 winPos = ImGui::GetMainViewport()->WorkPos;
	ImVec2 winSize = ImGui::GetMainViewport()->WorkSize;
	ImRect layout { winPos, winSize };

	// Draw the left-hand outline panel
	// TODO: widget-picker panel (handled via popup for now)
	ImRect outlineRect = Draw::RectCut(layout, winSize.x / 4.f, Draw::RectSide_Left);
	Draw::BeginWindow(outlineRect, "Outline");
	DrawOutlinePanel();
	ImGui::End();

	// Draw the right-hand details panel
	ImRect detailsRect = Draw::RectCut(layout, winSize.x / 4.f, Draw::RectSide_Right);
	m_detailsPane->SetLayoutArea(detailsRect);
	m_detailsPane->Draw();

	DrawLayoutView(layout);

	ImGui::PopStyleVar();

	if (m_metricsWindow)
		ImGui::ShowMetricsWindow(&m_metricsWindow);

	if (m_undoWindow)
		DrawUndoStack();

	if (m_debugWindow)
		DrawDebugWindow();
}

void MFDEditor::DrawOutlinePanel()
{
	// TODO: draw tools header

	DrawToolbar();

	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginChild("##ObjectHierarchy", {}, true);

	ImGui::Unindent(); // remove the initial indent level from the root object

	if (!DrawOutlineEntry(m_rootObject)) {
		ImGui::EndChild();
		return;
	}

	std::vector<std::pair<UIObject *, size_t>> objectStack;
	objectStack.reserve(32);
	objectStack.emplace_back(m_rootObject, 0);

	while (!objectStack.empty()) {
		auto &pair = objectStack.back();

		// finished with this node's children, close the tree node and continue
		if (pair.first->children.size() == pair.second) {
			objectStack.pop_back();
			ImGui::TreePop();
			continue;
		}

		UIObject *current = pair.first->children[pair.second++].get();

		if (DrawOutlineEntry(current))
			objectStack.emplace_back(current, 0);
	}

	ImGui::EndChild();
}

bool MFDEditor::DrawOutlineEntry(UIObject *obj)
{
	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanFullWidth;

	if (obj->children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	if (obj == m_selectedObject)
		flags |= ImGuiTreeNodeFlags_Selected;

	std::string label = fmt::format("[{}] {}", obj->id, obj->label);

	bool open = ImGui::TreeNodeEx(label.c_str(), flags);

	bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	bool context = ImGui::IsItemClicked(ImGuiMouseButton_Right);

	// TODO: render any other object information (icons etc.)

	if (clicked) {

		// TODO: handle drag/drop to reorder items in the list

		SetSelectedObject(obj);
	}

	if (context) {
		// TODO: open node context menu as needed
	}

	return open && !obj->children.empty();
}

void MFDEditor::DrawToolbar()
{
	bool isSelected = m_selectedObject != nullptr;
	bool isRoot = m_selectedObject == m_rootObject;
	float fontSize = ImGui::GetFontSize();

	ImVec2 btnSize = ImVec2{ fontSize, fontSize } + ImGui::GetStyle().FramePadding * 2.0f;

	ImGui::BeginGroup();

	if (isSelected) {

		UIObject *parent = m_selectedObject->parent;
		size_t childIdx = 0;

		if (parent) {
			for (size_t idx = 0; idx < parent->children.size(); idx++) {
				if (parent->children[idx].get() == m_selectedObject) {
					childIdx = idx;
					break;
				}
			}
		}

		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Horizontal;

		if (ImGui::Button("A##AddChild", btnSize)) {
			UIObject *child = CreateNewObject();
			child->label = StringName(fmt::format("object_{}", child->id));

			GetUndo()->BeginEntry("Add Child");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(m_selectedObject, child);
			GetUndo()->EndEntry();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add Empty Child");

		if (ImGui::Button("H##AddHBox", btnSize)) {
			UIObject *child = CreateNewObject();
			child->label = StringName(fmt::format("hbox_{}", child->id));
			child->alignment[0] = UIAlign_Fill;
			child->alignment[1] = UIAlign_Fill;

			GetUndo()->BeginEntry("Add HBox");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(m_selectedObject, child);
			GetUndo()->EndEntry();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add Horizontal Box");

		if (ImGui::Button("V##AddVBox", btnSize)) {
			UIObject *child = CreateNewObject();
			child->primaryAxis = UIAxis_Vertical;
			child->label = StringName(fmt::format("vbox_{}", child->id));
			child->alignment[0] = UIAlign_Fill;
			child->alignment[1] = UIAlign_Fill;

			GetUndo()->BeginEntry("Add VBox");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(m_selectedObject, child);
			GetUndo()->EndEntry();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add Vertical Box");

		if (ImGui::Button("T##AddText", btnSize)) {
			UIObject *child = CreateNewObject();
			child->primaryAxis = UIAxis_Vertical;
			child->label = StringName(fmt::format("text_{}", child->id));
			child->SetContentText("");

			GetUndo()->BeginEntry("Add Text");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(m_selectedObject, child);
			GetUndo()->EndEntry();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add Text");

		if (!isRoot && ImGui::Button("D##Delete", btnSize)) {

			GetUndo()->BeginEntry("Delete Child");
			GetUndo()->AddUndoStep<UndoAddRemoveChild>(parent, childIdx);
			GetUndo()->EndEntry();

			// Select the next child, previous child, or parent
			size_t numChildren = parent->children.size();
			if (childIdx < numChildren)
				SetSelectedObject(parent->children[childIdx].get());
			else if (childIdx > 0 && numChildren > 0)
				SetSelectedObject(parent->children[childIdx - 1].get());
			else
				SetSelectedObject(parent);
		}

		if (!isRoot && ImGui::IsItemHovered())
			ImGui::SetTooltip("Delete Selected Object");

		if (parent && childIdx > 0) {
			if (ImGui::Button("<##MovePrevious", btnSize)) {
				GetUndo()->BeginEntry("Reorder Child");
				GetUndo()->AddUndoStep<UndoReorderChild>(parent, childIdx, childIdx - 1);
				GetUndo()->EndEntry();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Reorder Previous");
		}

		if (parent && (childIdx + 1) < parent->children.size()) {
			if (ImGui::Button(">##MoveNext", btnSize)) {
				GetUndo()->BeginEntry("Reorder Child");
				GetUndo()->AddUndoStep<UndoReorderChild>(parent, childIdx, childIdx + 1);
				GetUndo()->EndEntry();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Reorder Next");
		}

		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Vertical;
	}

	ImGui::EndGroup();

}

void MFDEditor::DrawUndoStack()
{
	if (!ImGui::Begin("Undo Stack", &m_undoWindow, 0)) {
		ImGui::End();
		return;
	}

	ImGui::Text("Undo Depth: %ld", GetUndo()->GetEntryDepth());
	ImGui::Separator();

	size_t numEntries = m_undoSystem->GetNumEntries();
	size_t currentIdx = m_undoSystem->GetCurrentEntry();
	size_t selectedIdx = currentIdx;

	if (ImGui::Selectable("<Initial State>", currentIdx == 0))
		selectedIdx = 0;

	for (size_t idx = 0; idx < numEntries; idx++)
	{
		const UndoEntry *entry = m_undoSystem->GetEntry(idx);

		bool isSelected = currentIdx == idx + 1;
		std::string label = fmt::format("{}##{}", entry->GetName(), idx);

		if (ImGui::Selectable(label.c_str(), isSelected))
			selectedIdx = idx + 1;
	}

	ImGui::End();

	// If we selected an earlier history entry, undo to that point
	for (; currentIdx > selectedIdx; --currentIdx)
		m_undoSystem->Undo();

	// If we selected a later history entry, redo to that point
	for (; currentIdx < selectedIdx; ++currentIdx)
		m_undoSystem->Redo();
}

void MFDEditor::DrawDebugWindow()
{
	if (!ImGui::Begin("Debug Window", &m_debugWindow)) {
		ImGui::End();
		return;
	}

	if (m_selectedObject) {
		ImGui::TextUnformatted("Selected Object");

		ImGui::InputFloat2("Comp. Pos", &m_selectedObject->computedPos.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat2("Comp. Size", &m_selectedObject->computedSize.x, "%.3f", ImGuiInputTextFlags_ReadOnly);

		ImGui::Separator();
	}

	ImGui::ShowFontAtlas(m_rootView->GetFontAtlas());

	ImGui::End();
}

// ============================================================================
//  Layout window drawing functions
// ============================================================================

void MFDEditor::DrawLayoutView(ImRect layout)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 1.f, 1.f });
	Draw::BeginWindow(layout, "Viewport");
	ImGui::PopStyleVar(1);

	ImGuiStyle &style = ImGui::GetStyle();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 region = ImGui::GetContentRegionAvail();
	ImDrawList *dl = ImGui::GetWindowDrawList();

	m_viewportScreenPos = pos;

	// draw layout window outline
	dl->AddRectFilled(pos, pos + region, ImColor(style.Colors[ImGuiCol_ChildBg]));
	dl->AddRect(pos, pos + region, ImColor(style.Colors[ImGuiCol_Border]));

	// Draw layout window grid here
	dl->PushClipRect(pos, pos + region);

	const ImU32 col = IM_COL32(80, 80, 80, 255);
	const ImU32 primaryCol = IM_COL32(100, 100, 100, 255);

	ImVec2 &scroll = m_viewportScroll;
	float spacing = std::max(ceilf(GRID_SPACING * m_viewportZoom), GRID_SPACING);

	for (float x = fmodf(scroll.x, spacing); x < region.x; x += spacing)
		dl->AddLine(pos + ImVec2(x, 0), pos + ImVec2(x, region.y), col);
	for (float y = fmodf(scroll.y, spacing); y < region.y; y += spacing)
		dl->AddLine(pos + ImVec2(0, y), pos + ImVec2(region.x, y), col);

	// add primary grid line so we don't lose the center of the grid too badly
	dl->AddLine(pos + ImVec2(scroll.x, 0), pos + ImVec2(scroll.x, region.y), primaryCol);
	dl->AddLine(pos + ImVec2(0, scroll.y), pos + ImVec2(region.x, scroll.y), primaryCol);

	ImGui::BeginChild("##ViewportTools", region, false,
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_AlwaysUseWindowPadding);

	// set Horizontal layout type since we're using this window effectively as a toolbar
	ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Horizontal;

	// Draw any development-mode helpers here onto the viewport region

	ImGui::EndChild();

	// Update mouse down state, etc; handle active layout area interaction

	ImGuiButtonFlags flags =
		ImGuiButtonFlags_FlattenChildren |
		ImGuiButtonFlags_PressedOnClick |
		ImGuiButtonFlags_MouseButtonMask_;

	ImRect area = { pos, pos + region };

	ImGuiID viewportID = ImGui::GetID("ViewportContents");
	bool wasPressed = m_viewportActive;
	bool clicked = ImGui::ButtonBehavior(area, viewportID,
		&m_viewportHovered, &m_viewportActive, flags);

	ImGui::KeepAliveID(viewportID);

	// if the viewport is hovered/active or we just released it,
	// update mouse interactions with it
	if (m_viewportHovered || m_viewportActive || wasPressed) {
		// restrict the mouse pos within the viewport and convert to viewport-relative coords
		m_viewportMousePos = ImClamp(ImGui::GetIO().MousePos, area.Min, area.Max) - area.Min;

		HandleViewportInteraction(clicked, wasPressed);
	} else {
		m_viewportMousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	}

	// Draw debug info over the layout area
	std::string dbg = fmt::format("hovered: {}, active: {}, button: {}, zoom: {}",
		m_viewportHovered, m_viewportActive, ImGui::GetCurrentContext()->ActiveIdMouseButton, m_viewportZoom);
	ImGui::GetWindowDrawList()->AddText(area.Min, IM_COL32(255, 255, 255, 255), dbg.c_str());

	// Draw the preview to the drawlist and offset vertex positions etc.

	dl->PushClipRect(area.Min, area.Max);
	dl->AddDrawCmd();

	int startCmd = dl->CmdBuffer.Size;
	int startVtx = dl->VtxBuffer.Size;

	DrawPreview(dl);

	// offset and scale vertex positions into screen size
	for (int vtx = startVtx; vtx < dl->VtxBuffer.Size; vtx++) {
		dl->VtxBuffer[vtx].pos *= m_viewportZoom;
		dl->VtxBuffer[vtx].pos += m_viewportScroll + m_viewportScreenPos;
	}

	for (int cmd = startCmd; cmd < dl->CmdBuffer.Size; cmd++) {
		ImRect clipRect = dl->CmdBuffer[cmd].ClipRect;

		// Convert clip rect into screen coordinates and clip with viewport for display
		clipRect.Min = ViewToScreen(clipRect.Min);
		clipRect.Max = ViewToScreen(clipRect.Max);
		clipRect.ClipWithFull(area);

		dl->CmdBuffer[cmd].ClipRect = clipRect.ToVec4();
	}

	dl->AddDrawCmd();

	UIObject *highlightObject = GetHoveredObject();
	if (highlightObject)
		DrawObjectHighlight(dl, highlightObject, IM_COL32(255, 128, 0, 255));

	if (m_selectedObject && m_selectedObject != highlightObject)
		DrawObjectHighlight(dl, m_selectedObject, IM_COL32(0, 128, 255, 255));

	dl->PopClipRect();

	ImGui::End();
}

void MFDEditor::HandleViewportInteraction(bool clicked, bool wasPressed)
{
	ImGuiMouseButton activeButton = ImGui::GetCurrentContext()->ActiveIdMouseButton;

	// handle mouse movement events

	if (m_viewportActive) {
		if (activeButton == ImGuiMouseButton_Middle) {
			// Pan the viewport
			m_viewportScroll += ImGui::GetIO().MouseDelta;
		}

		// handle mouse movement events for dragging

	} else {
		if (ImGui::GetIO().MouseWheel != 0) {
			float oldZoom = m_viewportZoom;
			m_viewportZoom = Clamp(m_viewportZoom + ImGui::GetIO().MouseWheel / 4.f, 0.25f, 4.0f);

			float mult = m_viewportZoom / oldZoom;
			m_viewportScroll = ImFloor(m_viewportScroll * mult);
		}
	}

	// handle mouse down events

	if (clicked && activeButton == ImGuiMouseButton_Left) {
		// handle click events for selection
		UIObject *clicked = GetHoveredObject();
		SetSelectedObject(clicked);

		m_detailsPane->CloseStyleEditor();
	}

	if (clicked && activeButton == ImGuiMouseButton_Right) {
		// TODO: summon context menu to edit the clicked widget
	}

	// handle mouse up events

	if (!m_viewportActive && wasPressed) {
		// handle release events for e.g. selection / dragging
	}

}

void MFDEditor::DrawPreview(ImDrawList *outputDl)
{
	// Temp drawing test
	// outputDl->AddRect(ImVec2(0.f, 0.f), ImVec2(400, 400), IM_COL32(200, 200, 255, 255), 5.f, 0, 3.f);

	// Render the actual UI layout to the drawing context

	// TODO: use a drawlist owned by the UIView with the correct UV data etc.

	m_rootView->Draw(outputDl);
}

void MFDEditor::DrawObjectHighlight(ImDrawList *outputDl, UIObject *obj, ImU32 col)
{
	// convert object rectangle into screen-relative drawlist coords
	ImRect rect = { ViewToScreen(obj->screenRect.Min), ViewToScreen(obj->screenRect.Max) };
	rect.Expand(4.f);

	outputDl->AddRect(rect.Min, rect.Max, col, 2.f, {}, 2.f);
}
