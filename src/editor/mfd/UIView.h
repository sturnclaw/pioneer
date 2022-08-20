// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "editor/EditorDraw.h"
#include "imgui/imgui.h"

#include "RefCounted.h"

#include <memory>
#include <map>
#include <string_view>

namespace Graphics {
	class Texture;
	class Renderer;
}

class PropertyMap;

namespace Editor {

struct UIStyle;
struct UIObject;

/*
 * UIView represents a single UI "screen" and is responsible for handling
 * all Styles and Objects associated with it, as well as providing user
 * interaction and rendering methods.
 */
class UIView {
public:
	/*
	 * FontKey is a simple identifier to index imgui fonts by their name and size.
	 */
	struct FontKey {
		uint32_t name_hash;
		uint32_t size;

		bool operator==(const FontKey &rhs) const
		{
			return name_hash == rhs.name_hash && size == rhs.size;
		}

		bool operator<(const FontKey &rhs) const
		{
			return name_hash < rhs.name_hash || (name_hash == rhs.name_hash && size < rhs.size);
		}
	};

	using StyleContainer = std::map<std::string, std::unique_ptr<UIStyle>>;
	using FontCache = std::map<FontKey, ImFont *>;

public:
	UIView(Graphics::Renderer *r);
	~UIView();

	ImVec2 GetViewSize() const { return m_viewSize; }
	void SetViewSize(ImVec2 size);

	UIObject *GetRoot() const { return m_rootObject.get(); }
	void SetRoot(UIObject *obj);

	// Return the UIObject under the given mouse position (for Editor usage)
	UIObject *GetObjectAtPoint(ImVec2 pos);

	// Return a reference to this view's style container
	StyleContainer &GetStyles() { return m_styles; }

	// Get the string key for the given style pointer in the style cache
	// This should be called infrequently as it is O(N) in the length of the style cache
	std::string_view GetStyleName(UIStyle *style);

	// Return a reference to this view's font cache
	FontCache &GetFontCache() { return m_fonts; }

	// Get the string key for the given font pointer in the font cache
	// This should be called infrequently as it is O(N) in the length of the font cache
	std::string_view GetFontName(ImFont *font);

	// Return a reference to the font atlas used for this UIView
	ImFontAtlas *GetFontAtlas() { return m_fontAtlas.get(); }

	// Perform animation updates and layout pass
	void Update(float deltaTime);

	// Draw the resulting layout to a
	void Draw(ImDrawList *dl);

	// Called when the user clicks within this UIView
	void OnMouseDown(ImGuiMouseButton button, ImVec2 pos);
	// Called when the user releases a prior click within this UIView
	void OnMouseUp(ImGuiMouseButton button, ImVec2 pos);
	// Called when the mouse is moved within this UIView or while clicked inside
	void OnMouseMotion(ImVec2 pos);

	// Specify the backing font file for the given font name
	void RegisterFontFile(std::string name, std::string ttfFile);

	// Load or return the specified font at the specified size
	ImFont *GetOrLoadFont(std::string_view name, float size);

private:
	void RebuildFontTexture();

private:

	std::unique_ptr<UIObject> m_rootObject;

	ImVec2 m_viewSize;

	ImVec2 m_lastMousePos;
	uint32_t m_lastActiveWidget;

	// TODO: font management should potentially be moved to the DrawContext rather than a UIView
	std::unique_ptr<ImFontAtlas> m_fontAtlas;
	std::unique_ptr<ImDrawListSharedData> m_drawSharedData;
	std::unique_ptr<Graphics::Texture> m_fontTexture;
	Graphics::Renderer *m_renderer;

	StyleContainer m_styles;
	FontCache m_fonts;
	RefCountedPtr<PropertyMap> m_varMap;

	std::map<std::string, std::string, std::less<>> m_fontFiles;
	std::map<uint32_t, std::string> m_fontNameCache;
};

} // namespace Editor
