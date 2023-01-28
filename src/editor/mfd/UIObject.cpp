// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "UIObject.h"

#include "EditorDraw.h"
#include "FloatComparison.h"
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
	contentType = ContentType_None;
	style = in_style;
	alignment[0] = alignment[1] = UIAlign_NoExpand;
}

void UIObject::SetContentText(std::string_view in_content)
{
	contentType = ContentType_Text;
	content = in_content;
}

void UIObject::CalcSize(UIObject *parent)
{
	if (sizeMode[0] == SizeMode_FromContent) {
		computedSize.x = contentSize.x + style->padding.x * 2;
	} else if (sizeMode[0] == SizeMode_Fixed) {
		computedSize.x = size.x;
	} else if (sizeMode[0] == SizeMode_ParentPct) {
		computedSize.x = parent->computedSize.x * size.x;
	} else if (sizeMode[0] == SizeMode_FromChildren) {
		computedSize.x = 0.f;
	}

	if (sizeMode[1] == SizeMode_FromContent) {
		computedSize.y = contentSize.y + style->padding.y * 2;
	} else if (sizeMode[1] == SizeMode_Fixed) {
		computedSize.y = size.y;
	} else if (sizeMode[1] == SizeMode_ParentPct) {
		computedSize.y = parent->computedSize.y * size.y;
	} else if (sizeMode[1] == SizeMode_FromChildren) {
		computedSize.y = 0.f;
	}

	if (!(parent->features & UIFeature_OverlayLayout)) {
		// Expand size along parent primary and secondary axes for fill mode
		UIAxis axis = parent->primaryAxis;
		UIAxis axis2 = parent->primaryAxis == UIAxis_Vertical ? UIAxis_Horizontal : UIAxis_Vertical;

		if (alignment[axis] == UIAlign_Fill) {
			// TODO: variable widget weights
			computedSize[axis] = std::max(computedSize[axis], parent->cachedFreeSize * 1.f);
		}

		// TODO: expansion along secondary axis overlaps with SizeMode_ParentPct
		if (alignment[axis2] == UIAlign_Fill) {
			computedSize[axis2] = std::max(computedSize[axis2],
				parent->computedSize[axis2] - parent->style->padding[axis2] * 2.f);
		}
	}
}

void UIObject::CalcContainerWeights()
{
	if (features & UIFeature_OverlayLayout || children.empty())
		return;

	// Gather total weights and available space for expandable children
	float cachedWeight = 0.f;
	float fixedSize = style->padding[primaryAxis] +
		style->containerSpacing * (children.size() - 1);

	// Use the previous frame's computedSize value from children to calculate
	// reserved size (avoids extremely complex constraint resolution)
	for (auto &child : children) {
		if (child->alignment[primaryAxis] == UIAlign_NoExpand)
			fixedSize += child->computedSize[primaryAxis];
		else
			cachedWeight += 1.f;
	}

	// Store widget free size as a weight-normalized value for minimal per-widget math
	cachedFreeSize = (computedSize[primaryAxis] - fixedSize) / cachedWeight;
}

void UIObject::CalcSizeFromChildren()
{
	ImVec2 totalSize = { 0.f, 0.f };
	for (auto &child : children)
		totalSize = ImMax(totalSize, child->computedSize);

	// If this object uses automatic layout, calculate the total size from all
	// children and the style
	if (!(features & UIFeature_OverlayLayout)) {
		float spacing = style->containerSpacing * (children.size() - 1);

		if (primaryAxis == UIAxis_Horizontal) {
			totalSize.x = spacing;
			for (auto &child : children)
				totalSize.x += child->computedSize.x;
		} else {
			totalSize.y = spacing;
			for (auto &child : children)
				totalSize.y += child->computedSize.y;
		}
	}

	// add any padding specified in this object's style
	totalSize += style->padding * 2;

	// update this widget's sizes
	if (sizeMode[0] == SizeMode_FromChildren)
		computedSize.x = std::max(computedSize.x, totalSize.x);
	if (sizeMode[1] == SizeMode_FromChildren)
		computedSize.y = std::max(computedSize.y, totalSize.y);
}

void UIObject::CalcContentSize(UIObject *parent)
{
	// Calculate the wanted size of our content.
	// TODO: text wrapping
	ImVec2 maxArea;

	if (parent)
		maxArea = parent->computedSize - parent->style->padding * 2;
	else
		maxArea = size - style->padding * 2;

	if (contentType == ContentType_Text && !content.empty()) {
		contentSize = style->font->CalcTextSizeA(style->fontSize,
			FLT_MAX, maxArea.x, content.c_str());
	} else if (contentType == ContentType_Image) {
		assert(false && "Image contents are not yet implemented!");
	} else {
		contentSize = { 0.f, 0.f };
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
		ImVec2 nextPos = style->padding;

		UIAxis secondAxis = primaryAxis == UIAxis_Vertical ? UIAxis_Horizontal : UIAxis_Vertical;
		float maxSize2 = computedSize[secondAxis] - style->padding[secondAxis];

		for (auto &child : children) {
			ImVec2 computedPos = nextPos;

			UIAlign align = child->alignment[primaryAxis];
			UIAlign align2 = child->alignment[secondAxis];
			float size = child->computedSize[primaryAxis];
			float size2 = child->computedSize[secondAxis];

			// Each widget receives a given amount of space based on its weight
			// (currently hardcoded to 1.f) and can expand or be positioned
			// inside that alloted size.
			float expand = align == UIAlign_NoExpand ? 0.f : cachedFreeSize - size;

			if (align != UIAlign_NoExpand)
				computedPos[primaryAxis] += CalcAlignment(align, size, size + expand);
			if (align2 != UIAlign_NoExpand)
				computedPos[secondAxis] += CalcAlignment(align2, size2, maxSize2);

			child->computedPos = computedPos;

			if (primaryAxis == UIAxis_Horizontal)
				nextPos.x += size + expand + style->containerSpacing;
			else
				nextPos.y += size + expand + style->containerSpacing;
		}
	}

	// Calculate text/image content position inside this object
	contentPos.x = CalcAlignment(contentAlign[0], contentSize.x + style->padding.x * 2, computedSize.x) + style->padding.x;
	contentPos.y = CalcAlignment(contentAlign[1], contentSize.y + style->padding.y * 2, computedSize.y) + style->padding.y;
}

void UIObject::Draw(UIView *view, ImDrawList *dl)
{
	assert(contentType != ContentType_Image && "Image contents are not yet implemented!");

	if (features & UIFeature_ClipOverflow)
		dl->PushClipRect(screenRect.Min, screenRect.Max);

	bool drawBorder = features & UIFeature_DrawBorder;
	bool drawBackground = features & UIFeature_DrawBackground;
	if (drawBorder || drawBackground)
		style->RenderFrame(dl, screenRect, drawBorder, drawBackground);

	if (contentType == ContentType_Text && !content.empty())
		style->RenderText(dl, content, screenRect.Min + contentPos);

	if (features & UIFeature_ClipOverflow)
		dl->PopClipRect();
}

void UIObject::AddChild(UIObject *child, size_t idx)
{
	assert(idx <= children.size() && "Cannot add a child to a non-contiguous index!");

	children.emplace(children.cbegin() + idx, child);
	child->parent = this;
}

UIObject *UIObject::RemoveChild(size_t idx)
{
	assert(idx < children.size() && "Child index to remove is not valid!");

	UIObject *child = children[idx].release();
	children.erase(children.cbegin() + idx);
	child->parent = nullptr;

	return child;
}

void UIObject::ReorderChild(size_t idx, size_t newIdx)
{
	// simply delete the child from the old index and emplace it at the new index
	UIObject *child = RemoveChild(idx);
	AddChild(child, newIdx);
}
