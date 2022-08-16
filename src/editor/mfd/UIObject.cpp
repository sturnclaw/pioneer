// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "UIObject.h"

#include "EditorDraw.h"
#include "imgui/imgui.h"

using namespace Editor;

static float CalcAlignment(UIAlign alignment, float size, float parentSize)
{
	if (alignment == UIAlign_End)
		return parentSize - size;
	else if (alignment == UIAlign_Center)
		return (parentSize - size) * 0.5f;

	return 0.f;
}

// ============================================================================

void UIStyle::RenderFrame(ImDrawList *dl, ImRect bb, bool drawBorder, bool drawBackground)
{
	if (drawBackground)
		dl->AddRectFilled(bb.Min, bb.Max, backgroundColor, borderRounding);

	if (drawBorder)
		dl->AddRect(bb.Min, bb.Max, borderColor, borderRounding, 0, borderThickness);
}

void UIStyle::RenderText(ImDrawList *dl, std::string_view text, ImVec2 screenPos, float wrapWidth)
{
	dl->AddText(font, fontSize, screenPos, color, text.data(), text.data() + text.size(), wrapWidth);
}

void UIStyle::RenderImage(ImDrawList *dl, ImTextureID image, ImRect bb, ImRect uvs)
{
	dl->AddImage(image, bb.Min, bb.Max, uvs.Min, uvs.Max, color);
}

// ============================================================================

UIObject::UIObject()
{
	// here so instantiation of std::unique_ptr overloads take place after UIObject is fully defined
}

UIObject::~UIObject()
{
	// ditto
}

void UIObject::Setup(uint32_t in_id, UIFeature in_features, UIStyle *in_style)
{
	id = in_id;
	features = in_features;
	contentType = ContentType_Text;
	style = in_style;
}

void UIObject::SetContentText(std::string_view in_content)
{
	contentType = ContentType_Text;
	content = in_content;
	contentSize = style->font->CalcTextSizeA(style->fontSize,
		FLT_MAX, 0.f, content.data(), content.data() + content.size());
}

void UIObject::CalcSize(UIObject *parent)
{
	if (sizeMode[0] == SizeMode_FromContent) {
		computedSize.x = size.x + style->padding.x;
	} else if (sizeMode[0] == SizeMode_Fixed) {
		computedSize.x = size.x;
	} else if (sizeMode[0] == SizeMode_ParentPct) {
		computedSize.x = parent->size.x * size.x;
	} // SizeMode_FromChildren will be handled in a separate layout pass

	if (sizeMode[1] == SizeMode_FromContent) {
		computedSize.y = size.y + style->padding.y;
	} else if (sizeMode[1] == SizeMode_Fixed) {
		computedSize.y = size.y;
	} else if (sizeMode[1] == SizeMode_ParentPct) {
		computedSize.y = parent->size.y * size.y;
	} // SizeMode_FromChildren will be handled in a separate layout pass
}

void UIObject::CalcSizeFromChildren()
{
	ImVec2 totalSize = { 0.f, 0.f };

	if (features & UIFeature_OverlayLayout) {
		// If this object isn't an automatically-laid out container, use the
		// largest size of our children
		for (auto &child : children)
			totalSize = ImMax(totalSize, child->computedSize);
	} else {
		// Otherwise, calculate the total size from all children and the style
		ImVec2 spacing = ImVec2(style->containerSpacing, style->containerSpacing);
		totalSize += spacing * (children.size() - 1);

		for (auto &child : children)
			totalSize += child->computedSize;
	}

	// add any padding specified in this object's style
	totalSize += style->padding * 2;

	if (sizeMode[0] == SizeMode_FromChildren)
		computedSize.x = totalSize.x;
	if (sizeMode[1] == SizeMode_FromChildren)
		computedSize.y = totalSize.y;
}

ImVec2 UIObject::CalcContentSize(ImVec2 parentSize)
{
	if (contentType == ContentType_Text) {
		return style->font->CalcTextSizeA(style->fontSize, FLT_MAX, parentSize.x, content.c_str());
	} else {
		assert(false && "Image contents are not yet implemented!");
	}
}

void UIObject::Layout()
{
	// if we use overlay layout mode, just position all children inside the
	// container according to their alignment mode
	if (features & UIFeature_OverlayLayout) {
		ImVec2 offset = style->padding;
		ImVec2 size = computedSize - offset;

		// position children inside this container
		for (auto &child : children) {
			child->computedPos.x = CalcAlignment(child->alignment[0], child->computedSize.x, size.x);
			child->computedPos.y = CalcAlignment(child->alignment[1], child->computedSize.y, size.y);
		}
	} else {
		// otherwise, arrange all children inside the container along the primary axis
		// TODO: handle objects which can expand to take up free size in the container
		ImVec2 nextPos = computedPos + style->padding;

		for (auto &child : children) {
			child->computedPos = nextPos;

			if (primaryAxis == UIAxis_Horizontal)
				nextPos.x += child->computedSize.x + style->containerSpacing;
			else
				nextPos.y += child->computedSize.y + style->containerSpacing;
		}
	}

	// Calculate text/image content position inside this object
	contentPos.x = CalcAlignment(contentAlign[0], contentSize.x, computedSize.x);
	contentPos.y = CalcAlignment(contentAlign[1], contentSize.y, computedSize.y);
}

void UIObject::Draw(UIView *view, ImDrawList *dl)
{
	assert(contentType == ContentType_Text && "Image contents are not yet implemented!");

	bool drawBorder = features & UIFeature_DrawBorder;
	bool drawBackground = features & UIFeature_DrawBackground;
	if (drawBorder || drawBackground)
		style->RenderFrame(dl, screenRect, drawBorder, drawBackground);

	if (contentType == ContentType_Text && !content.empty())
		style->RenderText(dl, content, screenRect.Min + contentPos);
}
