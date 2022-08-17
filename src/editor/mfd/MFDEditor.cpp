// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "MFDEditor.h"

#include "UIObject.h"
#include "UIView.h"

#include "EditorApp.h"
#include "EditorDraw.h"
#include "UndoSystem.h"
#include "UndoStepType.h"
#include "graphics/Graphics.h"
#include "imgui/imgui.h"

using namespace Editor;

constexpr float GRID_SPACING = 10.f;

MFDEditor::MFDEditor(EditorApp *app) :
	Application::Lifecycle(),
	m_app(app),
	m_viewportMousePos(-FLT_MAX, -FLT_MAX),
	m_viewportScroll(100.f, 100.f),
	m_viewportZoom(1.f),
	m_selectedObject(nullptr),
	m_metricsWindow(false)
{
	m_undoSystem.reset(new UndoSystem());
}

MFDEditor::~MFDEditor()
{
}

UndoSystem *MFDEditor::GetUndo()
{
	return m_undoSystem.get();
}

void MFDEditor::Start()
{
	m_lastId = 0;

	m_rootView.reset(new UIView(m_app->GetRenderer()));

	// Register the default font files
	m_rootView->RegisterFontFile("pionillium", "PionilliumText22L-Medium.ttf");

	// Setup the "editor default" style
	m_defaultStyle = new UIStyle();
	m_defaultStyle->fontSize = 16;
	m_defaultStyle->font = m_rootView->GetOrLoadFont("pionillium", 16);
	m_defaultStyle->borderThickness = 2.f;

	// Register it in the style table
	m_rootView->GetStyles()["default"].reset(m_defaultStyle);

	// Setup a default size for this view
	m_rootView->SetViewSize(ImVec2(800, 600));

	// Setup the root object for this view
	m_rootObject = new UIObject();
	m_rootObject->Setup(m_lastId++, UIFeature_DrawBorder, m_defaultStyle);
	m_rootObject->label = "root"_name;

	m_rootView->SetRoot(m_rootObject);
}

void MFDEditor::Update(float deltaTime)
{
	// Note: this is janky as heck, should try to ensure Input works correctly even while ImGui has keyboard focus
	if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(SDL_SCANCODE_Z)) {
		m_undoSystem->Redo();
	} else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Z)) {
		m_undoSystem->Undo();
	}

	m_rootView->Update(deltaTime);

	DrawInterface();
}

void MFDEditor::End()
{
	m_lastId = 0;
	m_rootObject = nullptr;
	m_defaultStyle = nullptr;
	m_rootView.reset();
}

void MFDEditor::DrawInterface()
{
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("Tools")) {
		ImGui::Checkbox("Metrics Window", &m_metricsWindow);
		ImGui::Checkbox("Undo Stack", &m_undoWindow);
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
	Draw::BeginWindow(detailsRect, "Details");
	DrawDetailsPanel();
	ImGui::End();

	DrawLayoutView(layout);

	ImGui::PopStyleVar();

	if (m_metricsWindow)
		ImGui::ShowMetricsWindow(&m_metricsWindow);

	if (m_undoWindow)
		DrawUndoStack();
}

void MFDEditor::DrawOutlinePanel()
{
	// TODO: draw tools header

	ImGui::Unindent(); // remove the initial indent level from the root object

	if (!DrawOutlineEntry(m_rootObject))
		return;

	std::vector<std::pair<UIObject *, size_t>> objectStack;
	objectStack.reserve(32);
	objectStack.emplace_back(m_rootObject, 0);

	while (!objectStack.empty()) {
		auto pair = objectStack.back();

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
}

bool MFDEditor::DrawOutlineEntry(UIObject *obj)
{
	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanFullWidth;

	if (obj->children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf;

	if (obj == m_selectedObject)
		flags |= ImGuiTreeNodeFlags_Selected;

	std::string label = fmt::format("[{}] {}", obj->id, obj->label);

	bool open = ImGui::TreeNodeEx(label.c_str(), flags);

	bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	bool context = ImGui::IsItemClicked(ImGuiMouseButton_Right);

	// TODO: render any other object information (icons etc.)

	if (clicked) {
		SetSelectedObject(obj);
	}

	if (context) {
		// TODO: open node context menu as needed
	}

	return open;
}

void MFDEditor::DrawDetailsPanel()
{
	if (m_selectedObject) {
		DrawObjectDetails();
		return;
	}

	// TODO: handle this through the undo system

	ImVec2 viewSize = m_rootView->GetViewSize();
	bool changed = ImGui::InputFloat2("View Size", &viewSize.x);

	if (changed) {
		m_rootView->SetViewSize(viewSize);
	}

	UIObject *hovered = GetHoveredObject();
	ImGui::Text("Hovered: %d", hovered ? hovered->id : -1);
}

void MFDEditor::DrawObjectDetails()
{
	ImGui::Text("Label: %s", m_selectedObject->label.c_str());

	std::string_view styleName = m_rootView->GetStyleName(m_selectedObject->style);
	ImGui::Text("Style: %s", styleName.data());

	ImGui::InputFloat2("Size", &m_selectedObject->size.x);

	ImGui::InputFloat2("CPos", &m_selectedObject->computedPos.x);
	ImGui::InputFloat2("CSize", &m_selectedObject->computedSize.x);
}

void MFDEditor::DrawLayoutView(ImRect layout)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 1.f, 1.f });
	Draw::BeginWindow(layout, "Viewport");
	ImGui::PopStyleVar(1);

	ImGuiStyle &style = ImGui::GetStyle();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 region = ImGui::GetContentRegionAvail();
	ImDrawList *dl = ImGui::GetWindowDrawList();

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

	bool wasPressed = m_viewportActive;
	bool clicked = ImGui::ButtonBehavior(area, ImGui::GetID("ViewportContents"),
		&m_viewportHovered, &m_viewportActive, flags);

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

	dl->AddDrawCmd();
	int startVtx = dl->VtxBuffer.Size;

	DrawPreview(dl);

	// offset and scale vertex positions into screen size
	for (int vtx = startVtx; vtx < dl->VtxBuffer.Size; vtx++) {
		dl->VtxBuffer[vtx].pos *= m_viewportZoom;
		dl->VtxBuffer[vtx].pos += pos + m_viewportScroll;
	}

	ImGui::End();
}

void MFDEditor::DrawUndoStack()
{
	if (!ImGui::Begin("Undo Stack", &m_undoWindow, 0))
		ImGui::End();

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

void MFDEditor::SetSelectedObject(UIObject *obj)
{
	GetUndo()->BeginEntry("Change Selection");

	AddUndoSingleValue(GetUndo(), &m_selectedObject);
	m_selectedObject = obj;

	GetUndo()->EndEntry();
}

UIObject *MFDEditor::GetHoveredObject()
{
	// convert viewport position into View coordinates
	ImVec2 viewPos = (m_viewportMousePos - m_viewportScroll) / m_viewportZoom;

	return m_rootView->GetObjectAtPoint(viewPos);
}
