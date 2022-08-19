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
#include "imgui/imgui_stdlib.h"

using namespace Editor;

// Copied from imgui_demo.cpp
namespace ImGui { void ShowFontAtlas(ImFontAtlas* atlas); }

namespace {

	bool LayoutHorizontal(std::string_view label, int N, float reserveSize)
	{
		float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		float width = ImGui::GetContentRegionAvail().x;

		ImGui::TextUnformatted(label.data());

		ImGui::PushID(label.data());
		ImGui::PushItemWidth((width / N) - reserveSize - spacing);
		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Horizontal;

		return true;
	}

	void EndLayout()
	{
		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Vertical;
		ImGui::PopItemWidth();
		ImGui::PopID();
		ImGui::NewLine();
	}

	template<typename Closure>
	bool UndoHelper(std::string_view label, UndoSystem *undo, Closure update)
	{
		if (ImGui::IsItemDeactivated()) {
			undo->EndEntry();
			// Log::Info("Ending entry {}\n", label);
			update();
		}

		if (ImGui::IsItemActivated()) {
			undo->BeginEntry(label);
			// Log::Info("Beginning entry {}\n", label);
			return true;
		}

		if (ImGui::IsItemEdited())
			update();

		return false;
	}

	bool ComboUndoHelper(std::string_view undoName, const char *label, const char *preview, UndoSystem *undo)
	{
		ImGuiID id = ImGui::GetID(undoName.data());
		bool *opened = ImGui::GetStateStorage()->GetBoolRef(id);
		bool append = ImGui::BeginCombo(label, preview);

		if (ImGui::IsWindowAppearing()) {
			// Log::Info("Beginning entry {}\n", undoName);
			undo->BeginEntry(undoName);
			*opened = true;
		}

		if (*opened && !append)
		{
			*opened = false;
			undo->EndEntry();
			// Log::Info("Ending entry {}\n", undoName);
		}

		return append;
	}

	bool ComboUndoHelper(std::string_view label, const char *preview, UndoSystem *undo)
	{
		return ComboUndoHelper(label, label.data(), preview, undo);
	}

	template<typename T>
	struct SpanHelper {
		template<size_t N>
		SpanHelper(T (&arr)[N]) :
			data(arr),
			size(N)
		{}

		T *data;
		size_t size;
	};

	template<typename T>
	void EditOptions(std::string_view label, const char *name, SpanHelper<const char * const>options, UndoSystem *undo, T *val)
	{
		size_t selected = size_t(*val);
		const char *preview = selected < options.size ? options.data[selected] : "<invalid>";
		if (ComboUndoHelper(label, name, preview, undo)) {
			if (ImGui::IsWindowAppearing())
				AddUndoSingleValue(undo, val);

			for (size_t idx = 0; idx < options.size; ++idx) {
				if (ImGui::Selectable(options.data[idx], selected == idx))
					*val = T(idx);
			}

			ImGui::EndCombo();
		}
	}

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

	class UndoAddRemoveChild : public UndoStep {
	public:
		UndoAddRemoveChild(UIObject *parent, UIObject *add, size_t idx) :
			m_parent(parent),
			m_add(add),
			m_idx(idx)
		{
			Swap();
		}

		UndoAddRemoveChild(UIObject *parent, UIObject *add) :
			m_parent(parent),
			m_add(add),
			m_idx(parent->children.size())
		{
			Swap();
		}

		UndoAddRemoveChild(UIObject *parent, size_t idx) :
			m_parent(parent),
			m_add(nullptr),
			m_idx(idx)
		{
			Swap();
		}

		void Undo() override { Swap(); }
		void Redo() override { Swap(); }

	private:
		void Swap() {
			if (m_add) {
				m_parent->AddChild(m_add.release(), m_idx);
			} else {
				m_add.reset(m_parent->RemoveChild(m_idx));
			}
		}

		UIObject *m_parent;
		std::unique_ptr<UIObject> m_add;
		size_t m_idx;
	};

	class UndoReorderChild : public UndoStep {
	public:
		UndoReorderChild(UIObject *parent, size_t oldIdx, size_t newIdx) :
			m_parent(parent),
			m_old(oldIdx),
			m_new(newIdx)
		{
			Redo();
		}

		void Undo() override { m_parent->ReorderChild(m_new, m_old); }
		void Redo() override { m_parent->ReorderChild(m_old, m_new); }

	private:
		UIObject *m_parent;
		size_t m_old;
		size_t m_new;
	};

}


constexpr float GRID_SPACING = 10.f;

MFDEditor::MFDEditor(EditorApp *app) :
	Application::Lifecycle(),
	m_app(app),
	m_viewportMousePos(-FLT_MAX, -FLT_MAX),
	m_viewportScroll(100.f, 100.f),
	m_viewportZoom(1.f),
	m_selectedObject(nullptr),
	m_nextObject(nullptr),
	m_rootObject(nullptr),
	m_viewportHovered(false),
	m_viewportActive(false),
	m_metricsWindow(false),
	m_undoWindow(false),
	m_debugWindow(false)
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

	if (m_nextObject) {
		m_selectedObject = m_nextObject;
		m_nextObject = nullptr;
	}
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
	Draw::BeginWindow(detailsRect, "Details");
	DrawDetailsPanel();
	ImGui::End();

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

	ImGui::Unindent(); // remove the initial indent level from the root object

	if (!DrawOutlineEntry(m_rootObject))
		return;

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

void MFDEditor::DrawDetailsPanel()
{
	// TODO: tabbed layout containing object details, styles, and vars

	if (m_selectedObject) {
		DrawObjectDetails(m_selectedObject->parent, m_selectedObject);
		return;
	}

	ImVec2 val = m_rootView->GetViewSize();
	ImGui::InputFloat2("View Size", &val.x, "%.1f");

	if (UndoHelper("Edit View Size", GetUndo(), [=](){ m_rootView->SetViewSize(val); }))
		AddUndoGetSetValue<&UIView::GetViewSize, &UIView::SetViewSize>(GetUndo(), m_rootView.get(), val);

	UIObject *hovered = GetHoveredObject();
	ImGui::Text("Hovered: %d", hovered ? hovered->id : -1);
}

void MFDEditor::DrawObjectDetails(UIObject *parent, UIObject *obj)
{
	std::string label = std::string(obj->label.sv());
	ImGui::InputText("Label", &label);

	if (UndoHelper("Edit Label", GetUndo(), [=](){ obj->label = StringName(label); }))
		AddUndoSingleValue(GetUndo(), &obj->label, StringName(label));

	uint32_t features = obj->features;
	bool changed = false;
	if (ComboUndoHelper("Edit Features", "Features", GetUndo())) {
		if (ImGui::IsWindowAppearing())
			AddUndoSingleValue(GetUndo(), &obj->features);

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

	std::string_view styleName = m_rootView->GetStyleName(obj->style);
	if (ImGui::BeginCombo("Style", styleName.data())) {

		// TODO: maintain list of styles, possibly loaded from a file
		ImGui::Selectable(styleName.data(), true);

		ImGui::EndCombo();
	}

	if (parent) {

		ImGui::Separator();

		ImVec2 _size = obj->size;
		ImGui::InputFloat2("Size", &_size.x);
		if (UndoHelper("Edit Size", GetUndo(), [=](){ obj->size = _size; }))
			AddUndoSingleValue(GetUndo(), &obj->size, _size);

		if (LayoutHorizontal("Size Mode:", 2, ImGui::GetFontSize())) {
			EditOptions("Edit Size Mode X", "X", SizeModes, GetUndo(), &obj->sizeMode[0]);
			EditOptions("Edit Size Mode Y", "Y", SizeModes, GetUndo(), &obj->sizeMode[1]);

			EndLayout();
		}

		if (parent->features & UIFeature_OverlayLayout) {

			if (LayoutHorizontal("Alignment Mode:", 2, ImGui::GetFontSize())) {
				EditOptions("Edit Alignment X", "X", AlignModes, GetUndo(), &obj->alignment[0]);
				EditOptions("Edit Alignment Y", "Y", AlignModes, GetUndo(), &obj->alignment[1]);

				EndLayout();
			}

		} else {

			if (LayoutHorizontal("Expansion Mode:", 2, ImGui::GetFontSize())) {
				EditOptions("Edit Expansion X", "X", ExpandModes, GetUndo(), &obj->alignment[0]);
				EditOptions("Edit Expansion Y", "Y", ExpandModes, GetUndo(), &obj->alignment[1]);

				EndLayout();
			}

		}

	}

	if (!(obj->features & UIFeature_OverlayLayout)) {
		EditOptions("Edit Primary Axis", "PrimaryAxis", AxisModes, GetUndo(), &obj->primaryAxis);
	}

	ImGui::Separator();

	EditOptions("Edit Content Type", "Content Type", ContentTypes, GetUndo(), &obj->contentType);

	if (obj->contentType == ContentType_Text) {
		std::string content = obj->content;
		ImGui::InputText("Content", &content);

		if (UndoHelper("Edit Content", GetUndo(), [=](){ obj->content = content; }))
			AddUndoSingleValue(GetUndo(), &obj->content);
	}

	if (obj->contentType != ContentType_None) {
		if (LayoutHorizontal("Content Alignment:", 2, ImGui::GetFontSize())) {
			EditOptions("Edit Content Alignment X", "X", AlignModes, GetUndo(), &obj->contentAlign[0]);
			EditOptions("Edit Content Alignment Y", "Y", AlignModes, GetUndo(), &obj->contentAlign[1]);

			EndLayout();
		}
	}

	ImGui::Separator();

	ImGui::InputFloat2("Comp. Pos", &obj->computedPos.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat2("Comp. Size", &obj->computedSize.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
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

void MFDEditor::DrawUndoStack()
{
	if (!ImGui::Begin("Undo Stack", &m_undoWindow, 0))
		ImGui::End();

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

	ImGui::ShowFontAtlas(m_rootView->GetFontAtlas());

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

void MFDEditor::SetSelectedObject(UIObject *obj)
{
	// Defer object selection until end of the frame to avoid undo issues
	m_nextObject = obj;
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
