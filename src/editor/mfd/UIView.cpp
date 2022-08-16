// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "UIView.h"

#include "AnimationCurves.h"
#include "FileSystem.h"
#include "UIObject.h"

#include "core/FNV1a.h"
#include "core/Property.h"
#include "graphics/Renderer.h"
#include "graphics/Texture.h"
#include "imgui/imgui.h"

using namespace Editor;

UIView::UIView(Graphics::Renderer *r) :
	m_viewSize(0.f, 0.f),
	m_lastMousePos(-FLT_MAX, -FLT_MAX),
	m_lastActiveWidget(UINT32_MAX),
	m_renderer(r)
{
	m_fontAtlas.reset(new ImFontAtlas());

	m_drawSharedData.reset(new ImDrawListSharedData());
	m_drawSharedData->SetCircleTessellationMaxError(0.30f);
	m_drawSharedData->CurveTessellationTol = 1.25f;
	m_drawSharedData->InitialFlags =
		ImDrawListFlags_AntiAliasedLines |
		ImDrawListFlags_AntiAliasedLinesUseTex |
		ImDrawListFlags_AntiAliasedFill;

	if (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset)
		m_drawSharedData->InitialFlags |= ImDrawListFlags_AllowVtxOffset;
}

UIView::~UIView()
{
}

void UIView::SetViewSize(ImVec2 size)
{
	m_viewSize = size;

	if (m_rootObject)
		m_rootObject->size = size;
}

void UIView::SetRoot(UIObject *obj)
{
	m_rootObject.reset(obj);

	if (!obj)
		return;

	obj->size = m_viewSize;
	obj->sizeMode[0] = SizeMode_Fixed;
	obj->sizeMode[1] = SizeMode_Fixed;
}

UIObject *UIView::GetObjectAtPoint(ImVec2 pos)
{
	if (!m_rootObject)
		return nullptr;

	std::vector<UIObject *> searchStack = { m_rootObject.get() };
	UIObject *lastValid = nullptr;

	while (!searchStack.empty()) {
		UIObject *current = searchStack.back();
		searchStack.pop_back();

		if (!current->screenRect.Contains(pos))
			continue;

		if (current->children.empty())
			return current;

		// if none of our children contain the selection point, return this node
		lastValid = current;

		// push children on the stack in reverse order so we process the
		// child nodes in the original order
		for (int64_t idx = current->children.size() - 1; idx >= 0; --idx) {
			searchStack.push_back(current->children[idx].get());
		}
	}

	return lastValid;
}

std::string_view UIView::GetStyleName(UIStyle *style)
{
	for (auto &pair : m_styles)
		if (pair.second.get() == style)
			return pair.first;

	return {};
}

void UIView::Update(float deltaTime)
{
	if (!m_rootObject)
		return;

	// Update required parameters for layout of child objects
	m_rootObject->computedPos = {};
	m_rootObject->computedSize = m_rootObject->size;
	m_rootObject->screenRect = { m_rootObject->computedPos, m_rootObject->computedSize };

	if (m_rootObject->children.empty())
		return;

	// Calculate widget sizes and perform a layout pass on them

	std::vector<std::pair<UIObject *, size_t>> layoutStack;
	layoutStack.reserve(32);

	std::vector<UIObject *> postOrderStack;
	postOrderStack.reserve(32);

	// CalcSize / animation update pass (top-down)
	// This updates all objects which have size modes that are independent or
	// only depend on parent sizes.
	layoutStack.emplace_back(m_rootObject.get(), 0);
	while (!layoutStack.empty()) {
		auto &pair = layoutStack.back();

		UIObject *parent = pair.first;
		UIObject *current = parent->children[pair.second++].get();

		if (pair.second == parent->children.size())
			layoutStack.pop_back();

		// Update this object's hovered/active state
		if (current->features & (UIFeature_HoverAnim | UIFeature_ActiveAnim)) {
			bool hovered = current->screenRect.Contains(m_lastMousePos);
			bool active = current->id == m_lastActiveWidget;

			AnimationCurves::Approach(current->hoveredAnim, hovered ? 1.f : 0.f, deltaTime);
			AnimationCurves::Approach(current->activeAnim, active ? 1.f : 0.f, deltaTime);
		}

		// Inherit hovered/active state from parent (for e.g. styling)
		if (current->features & UIFeature_InheritAnim) {
			current->hoveredAnim = parent->hoveredAnim;
			current->activeAnim = parent->activeAnim;
		}

		// Calculate this object's size
		current->CalcSize(parent);

		if (current->sizeMode[0] == SizeMode_FromChildren || current->sizeMode[1] == SizeMode_FromChildren) {
			postOrderStack.push_back(current);
		}

		if (!current->children.empty())
			layoutStack.emplace_back(current, 0);
	}

	// Calculate size of objects which depend on their children's sizes (bottom-up)
	while (!postOrderStack.empty()) {
		postOrderStack.back()->CalcSizeFromChildren();
		postOrderStack.pop_back();
	}

	// Note: there is no constraint-solving attempted here, children can overflow their parents

	// Layout pass (top-down)
	layoutStack.emplace_back(m_rootObject.get(), 0);
	m_rootObject->Layout();

	while (!layoutStack.empty()) {
		auto &pair = layoutStack.back();

		UIObject *parent = pair.first;
		UIObject *current = parent->children[pair.second++].get();

		// if we're processing the last child, remove this entry from the list
		if (pair.second == parent->children.size())
			layoutStack.pop_back();

		current->Layout();

		ImVec2 screenPos = parent->screenRect.Min + current->computedPos;
		current->screenRect = { screenPos, screenPos + current->computedSize };

		if (!current->children.empty())
			layoutStack.emplace_back(current, 0);
	}
}

void UIView::Draw(ImDrawList *dl)
{
	RebuildFontTexture();

	m_fontAtlas->Locked = true;

	// Setup the full-screen clip rect for this drawlist
	m_drawSharedData->ClipRectFullscreen = ImVec4(0.f, 0.f, m_viewSize.x, m_viewSize.y);

	// Push the font atlas texture here for 'fast path' inside widget drawing code
	dl->PushTextureID(m_fontAtlas->TexID);

	std::vector<UIObject *> drawStack = { m_rootObject.get() };

	// Draw all widgets in the hierarchy
	while (!drawStack.empty()) {
		UIObject *current = drawStack.back();
		drawStack.pop_back();

		current->Draw(this, dl);

		if (!current->children.empty()) {
			for (auto &child : current->children)
				drawStack.push_back(child.get());
		}
	}

	dl->PopTextureID();

	m_fontAtlas->Locked = false;
}

void UIView::OnMouseDown(ImGuiMouseButton button, ImVec2 pos)
{
	// TODO: widget interaction
}

void UIView::OnMouseUp(ImGuiMouseButton button, ImVec2 pos)
{
	// TODO: widget interaction
}

void UIView::OnMouseMotion(ImVec2 pos)
{
	// TODO: widget interaction
}

void UIView::RegisterFontFile(std::string name, std::string ttfFile)
{
	m_fontFiles[name] = ttfFile;
}

void UIView::RebuildFontTexture()
{
	if (m_fontAtlas->IsBuilt() && m_fontTexture)
		return;

	m_fontAtlas->Build();

	uint8_t *pixels = nullptr;
	int width, height;
	m_fontAtlas->GetTexDataAsRGBA32(&pixels, &width, &height);

	vector3f dataSize = { (float)width, (float)height, 0 };
	if (!m_fontTexture || !(dataSize == m_fontTexture->GetDescriptor().dataSize)) {
		Graphics::TextureDescriptor desc(
			Graphics::TEXTURE_RGBA_8888, dataSize,
			Graphics::LINEAR_REPEAT,
			false, false, false, 0,
			Graphics::TEXTURE_2D);

		m_fontTexture.reset(m_renderer->CreateTexture(desc));
	}

	m_fontTexture->Update(pixels, dataSize, Graphics::TEXTURE_RGBA_8888);
	m_fontAtlas->TexID = ImTextureID(m_fontTexture.get());
}

ImFont *UIView::GetOrLoadFont(std::string_view name, float size)
{
	FontKey key;
	key.name_hash = hash_32_fnv1a(name.data(), name.size());
	key.size = uint32_t(size);

	auto iter = m_fonts.find(key);
	if (iter != m_fonts.end())
		return iter->second;

	assert(!m_fontAtlas->Locked && "Cannot load a new font while inside of UIView::Draw()");

	const ImWchar *glyphRanges = m_fontAtlas->GetGlyphRangesDefault();

	assert(m_fontFiles.count(name) && "Cannot load a font that has not been registered yet!");

	std::string ttfFile = FileSystem::JoinPath("fonts", m_fontFiles.find(name)->second);

	RefCountedPtr<FileSystem::FileData> fileData = FileSystem::gameDataFiles.ReadFile(ttfFile);

	// Make a copy of the font data so it persists past the lifetime of the file
	uint8_t *bytes = static_cast<uint8_t *>(std::malloc(fileData->GetSize()));
	std::memcpy(bytes, fileData->GetData(), fileData->GetSize());

	ImFont *font = m_fontAtlas->AddFontFromMemoryTTF(bytes, fileData->GetSize(), size, nullptr, glyphRanges);
	m_fonts.emplace(key, font);

	return font;
}
