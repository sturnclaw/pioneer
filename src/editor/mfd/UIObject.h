// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "core/StringName.h"
#include "editor/EditorDraw.h"
#include "imgui/imgui.h"

#include <string>
#include <map>
#include <memory>
#include <vector>

namespace Lang {
	class Resource;
} // namespace Lang

namespace Editor {

class UIView;

enum UIFeature : uint32_t {
	UIFeature_DrawBorder     = 1 << 0, // TODO: should this be part of the Style instead?
	UIFeature_DrawBackground = 1 << 1, // TODO: should this be part of the Style instead?
	UIFeature_Clickable      = 1 << 2, // This UIObject should be interactable and treated like a button
	UIFeature_Scrollable     = 1 << 3, // This UIObject should allow its contents to overflow and be scrollable along the primary axis
	UIFeature_HoverAnim      = 1 << 4, // This UIObject should tick an animation for hovered/not hovered
	UIFeature_ActiveAnim     = 1 << 5, // This UIObject should tick an animation for active/inactive
	UIFeature_OverlayLayout  = 1 << 6, // This UIObject does not lay out children along the primary axis
	UIFeature_InheritAnim    = 1 << 7, // This UIObject should inherit hovered/active state from the parent
	UIFeature_WrapText       = 1 << 8, // This UIObject should wrap its text contents based on the size of the parent
};

enum SizeMode : uint8_t {
	SizeMode_FromContent  = 0, // Object should be sized to its text/image content plus padding
	SizeMode_Fixed        = 1, // Object should have a fixed size
	SizeMode_ParentPct    = 2, // Object should use the given amount of the parent's size
	SizeMode_FromChildren = 3, // Object should use the sum of their children's sizes plus padding
};

enum ContentType : uint8_t {
	ContentType_None  = 0,
	ContentType_Text  = 1,
	ContentType_Image = 2,
};

enum UIAxis : uint8_t {
	UIAxis_Horizontal = 0,
	UIAxis_Vertical = 1
};

enum UIAlign : uint8_t {
	UIAlign_Start = 0,
	UIAlign_Center = 1,
	UIAlign_End = 2,
	UIAlign_Fill = 3,
	UIAlign_NoExpand = 4,
};

struct UIStyle {
	// TODO: 'partial' borders on specified sides only
	// TODO: different colors for different border sides
	// TODO: corner-specific rounding
	// TODO: blending for hovered/active colors and states (need sub-struct for "state style" containing border/color info?)

	ImFont *font = nullptr;
	float   fontSize = 16.f;

	ImU32   color = IM_COL32(255, 255, 255, 255);
	ImU32   backgroundColor = IM_COL32(0, 0, 0, 0);
	ImVec2  padding = { 0.f, 0.f };

	ImU32   borderColor = IM_COL32(255, 255, 255, 255);
	float   borderRounding = 0.f;
	float   borderThickness = 1.f;

	float   containerSpacing = 0.f;

	void RenderFrame(ImDrawList *dl, ImRect bb, bool drawBorder, bool drawBackground);

	void RenderText(ImDrawList *dl, std::string_view text, ImVec2 screenPos, float wrapWidth = 0.f);
	void RenderImage(ImDrawList *dl, ImTextureID image, ImRect bb, ImRect uvs);
};

/*
 * UIObject is a "fat" widget structure containing all needed common parameters
 * for layout and drawing any building block in an MFD or other UI structure.
 *
 * Typically, common semantic widgest like a checkbox will be composed of
 * multiple objects like so:
 *
 *  + Checkbox Container (container object)
 *    + Check box frame (frame + border)
 *      + Check mark (vector / text)
 *    + Label text (text / optional effects for hover)
 */
struct UIObject {
	UIObject();
	~UIObject();

	// "Hot" widget data, used during layout/drawing pass (32b)
	uint32_t            id = 0;
	UIFeature           features = {};

	ImVec2              computedPos = {};       // computed position of the object relative to parent
	ImVec2              computedSize = {};      // computed size of the object on axis N (according to sizeMode[N])

	ImVec2              size = {};              // the wanted size of this object or the size of the content
	SizeMode            sizeMode[2] = {};       // the sizing mode of the object on the given axis

	UIAlign             alignment[2] = {};      // the alignment of the object within the parent on the given axis
	UIAxis              primaryAxis = {};       // axis to layout children along (for container objects)

	ContentType         contentType = {};       // type of content to be displayed in this object
	UIAlign             contentAlign[2] = {};   // alignment of content within the object bounds

	// "Cold" widget data (modified once per frame or less) (96b)
	std::string         content;                // text string or image path for this object
	ImVec2              contentSize = {};       // expected size of the content, updated when content or style changes

	UIStyle            *style = nullptr;        // style used to draw this object
	StringName          label;                  // unique ID / editor label of this object. Passed to Lua for events

	float               hoveredAnim = 0.f;      // "hot" animation used for hover/focus/etc
	float               activeAnim = 0.f;       // "active" animation used for click, on/off etc

	uint64_t            _unused1;               // [ padding ]

	// cached position information for drawing (updated during Layout pass) (24b)
	ImRect              screenRect = {};        // screen-space rect of this object
	ImVec2              contentPos = {};        // offset of the content within the object according to alignment

	UIObject           *parent = nullptr;
	std::vector<std::unique_ptr<UIObject>> children;

	// Initialize this object
	void Setup(uint32_t id, UIFeature features, UIStyle *style);

	// Initialize this UIObject to display string contents
	void SetContentText(std::string_view content);

	// Render this object to a drawlist
	void Draw(UIView *view, ImDrawList *drawList);

	// Calculate the size of this object according to its size mode and optionally the size of its parent
	void CalcSize(UIObject *parent);

	// Calculate the size of this object according to the size of its children
	void CalcSizeFromChildren();

	// Update wanted size of this object from its content
	void CalcContentSize(UIObject *parent);

	// Layout all children of this object
	void Layout();

	// Add a new object as a child of this object
	// The given child is now owned by this object
	void AddChild(UIObject *child, size_t idx);

	// Remove and return the child at the given index
	// Deletion of the child is the responsibility of the parent code
	UIObject *RemoveChild(size_t idx);

	// Reorder the child at idx so it occupies newIdx in the array
	void ReorderChild(size_t idx, size_t newIdx);
};

} // namespace Editor
