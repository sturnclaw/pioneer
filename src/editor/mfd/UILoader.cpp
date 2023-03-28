// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "UILoader.h"

#include "UIObject.h"

#include "JsonUtils.h"

using namespace Editor;

namespace {

	constexpr const char *FeatureNames[] = {
		"DrawBorder",
		"DrawBackground",
		"Clickable",
		"Scrollable",
		"HoverAnim",
		"ActiveAnim",
		"OverlayLayout",
		"InheritAnim",
		"WrapText",
		"ClipOverflow"
	};

	constexpr const char *SizeModeNames[] = {
		"FromContent",
		"Fixed",
		"ParentPct",
		"FromChildren"
	};

	constexpr const char *ContentTypeNames[] = {
		"None",
		"Text",
		"Image"
	};

	constexpr const char *AlignNames[] = {
		"Start",
		"Center",
		"End",
		"Fill",
		"NoExpand"
	};

}

namespace Editor {

	void from_json(const Json &obj, UIFeature &var)
	{
		var = UIFeature(0);
		if (!obj.is_array())
			return;

		for (const auto &name : obj) {
			std::string_view feature = name.get<std::string_view>();
			for (size_t idx = 0; idx < std::size(FeatureNames); idx++) {
				if (feature == FeatureNames[idx]) {
					(uint32_t &)var |= UIFeature(1 << idx);
					return;
				}
			}
		}
	}

	void to_json(Json &obj, const UIFeature &var)
	{
		obj = Json::array();

		for (size_t idx = 0; idx < std::size(FeatureNames); idx++) {
			if (var & UIFeature(1 << idx))
				obj.push_back(FeatureNames[idx]);
		}
	}

	void from_json(const Json &obj, SizeMode &var)
	{
		std::string_view mode = obj.get<std::string_view>();
		for (size_t idx = 0; idx < std::size(SizeModeNames); idx++) {
			if (mode == SizeModeNames[idx]) {
				var = SizeMode(idx);
				return;
			}
		}
	}

	void to_json(Json &obj, const SizeMode &sm)
	{
		obj = SizeModeNames[sm];
	}

	void from_json(const Json &obj, UIAlign &var)
	{
		std::string_view align = obj.get<std::string_view>();
		for (size_t idx = 0; idx < std::size(AlignNames); idx++) {
			if (align == AlignNames[idx]) {
				var = UIAlign(idx);
				return;
			}
		}
	}

	void to_json(Json &obj, const UIAlign &var)
	{
		obj = AlignNames[var];
	}

	void from_json(const Json &obj, ContentType &var)
	{
		std::string_view type = obj.get<std::string_view>();
		for (size_t idx = 0; idx < std::size(ContentTypeNames); idx++) {
			if (type == ContentTypeNames[idx]) {
				var = ContentType(idx);
				return;
			}
		}
	}

	void to_json(Json &obj, const ContentType &var)
	{
		obj = ContentTypeNames[var];
	}

} // namespace Editor

void from_json(const Json &obj, ImVec2 &vec)
{
	vec = { obj[0].get<float>(), obj[1].get<float>() };
}

void to_json(Json &obj, const ImVec2 &vec)
{
	obj = Json::array({ vec.x, vec.y });
}

void from_json(const Json &obj, ImColor &col)
{
	col = ImColor(
		obj[0].get<int>(),
		obj[1].get<int>(),
		obj[2].get<int>(),
		obj[3].get<int>()
	);
}

void to_json(Json &obj, const ImColor &col)
{
	obj = Json::array({
		int(col.Value.x * 255),
		int(col.Value.y * 255),
		int(col.Value.z * 255),
		int(col.Value.w * 255)
	});
}

UILoader::UILoader(Delegate *delegate) :
	m_delegate(delegate)
{
}

UILoader::~UILoader()
{
}

Json UILoader::SaveObject(const UIObject *object)
{
	Json out = Json::object();

	out["label"] = object->label.sv();
	out["features"] = object->features;

	out["size"] = object->size;
	out["sizeMode"] = Json::array({ object->sizeMode[0], object->sizeMode[1] });
	out["alignment"] = Json::array({ object->alignment[0], object->alignment[1] });

	out["contentType"] = object->contentType;
	out["contentAlign"] = Json::array({ object->contentAlign[0], object->contentAlign[1] });

	if (object->contentType == ContentType_Text) {
		out["content"] = object->content;
	}

	out["style"] = m_delegate->GetStyleName(object->style);

	if (object->children.empty())
		return out;

	Json children = Json::array();
	for (const auto &child : object->children) {
		children.push_back(SaveObject(child.get()));
	}

	out["children"] = children;

	return out;
}

UIObject *UILoader::LoadObject(const Json &obj)
{
	UIObject *out = m_delegate->CreateNewObject();

	out->label = obj["label"].get<std::string_view>();
	out->features = obj["features"].get<UIFeature>();

	out->size = obj["size"];
	out->sizeMode[0] = obj["sizeMode"][0];
	out->sizeMode[1] = obj["sizeMode"][1];

	out->alignment[0] = obj["alignment"][0];
	out->alignment[1] = obj["alignment"][1];

	out->contentType = obj["contentType"];
	out->contentAlign[0] = obj["contentAlign"][0];
	out->contentAlign[1] = obj["contentAlign"][1];

	if (out->contentType == ContentType_Text)
		out->SetContentText(obj["content"].get<std::string_view>());

	out->style = m_delegate->GetStyle(obj["style"].get<std::string_view>());

	if (!obj.count("children"))
		return out;

	for (const auto &child : obj["children"]) {
		out->AddChild(LoadObject(child), out->children.size());
	}

	return out;
}

Json UILoader::SaveStyle(const UIStyle *style)
{
	Json out = Json::object();

	out["font"] = m_delegate->GetFontName(style->font);
	out["fontSize"] = style->fontSize;

	out["color"] = ImColor(style->color);
	out["bgColor"] = ImColor(style->backgroundColor);
	out["padding"] = style->padding;

	out["borderColor"] = ImColor(style->borderColor);
	out["borderRounding"] = style->borderRounding;
	out["borderThickness"] = style->borderThickness;

	out["containerSpacing"] = style->containerSpacing;

	return out;
}

UIStyle *UILoader::LoadStyle(const Json &obj)
{
	UIStyle *style = m_delegate->CreateNewStyle();

	std::string_view fontName = obj["font"].get<std::string_view>();
	style->fontSize = obj["fontSize"];
	style->font = m_delegate->GetFont(fontName, style->fontSize);

	style->color = obj["color"].get<ImColor>();
	style->backgroundColor = obj["bgColor"].get<ImColor>();
	style->padding = obj["padding"];

	style->borderColor = obj["borderColor"].get<ImColor>();
	style->borderRounding = obj["borderRounding"];
	style->borderThickness = obj["borderThickness"];

	style->containerSpacing = obj["containerSpacing"];

	return style;
}
