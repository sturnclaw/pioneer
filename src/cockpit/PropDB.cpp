// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "PropDB.h"

#include "InteractionScene.h"
#include "Modules.h"

#include "JsonUtils.h"
#include "Pi.h"
#include "lua/Lua.h"
#include "lua/LuaUtils.h"
#include "scenegraph/Model.h"
#include "scenegraph/Tag.h"

using namespace Cockpit;


void PropDB::LoadPropCtx()
{
	lua_State *l = Lua::manager->GetLuaState();

	lua_newtable(l);

	pi_lua_dofile(l, "cockpits/script/FlightController.lua", 1);
	lua_setfield(l, -2, "fc");

	// Set the metatable of this env table to point to the global environment
	lua_newtable(l);
	lua_getglobal(l, "_G");
	lua_setfield(l, -2, "__index");
	lua_setmetatable(l, -2);

	m_envTable = LuaRef(l, -1);
	lua_pop(l, 1);
}

void PropDB::LoadProps(const std::string &path)
{
	PROFILE_SCOPED()

	const Json &propFile = JsonUtils::LoadJsonDataFile(path);

	// FIXME: gracefully handle invalid prop file
	assert(propFile.is_array());

	for (const auto &item : propFile) {
		if (!item.is_object()) {
			Log::Warning("Invalid prop definition in file {}\n", path);
			continue;
		}

		std::string_view id = item["id"];

		// Ensure we're not registering a duplicate entry
		bool isTemplate = item.value("template", false);
		if (isTemplate) {
			if (m_templates.find(id) != m_templates.end()) {
				Log::Warning("Duplicate definition for prop template {}\n", id);
				continue;
			}
		} else {
			if (m_props.find(id) != m_props.end()) {
				Log::Warning("Duplicate definition for prop {}\n", id);
				continue;
			}
		}

		Json node = nullptr;

		// Handle inheriting from a template node definition
		if (item.count("inherit-from")) {
			std::string_view parent = item["inherit-from"];

			auto iter = m_templates.find(parent);
			if (iter == m_templates.end()) {
				Log::Warning("No parent definition {} for prop definition {}\n", parent, id);
				continue;
			}

			node = MergeTemplate(item, iter->second);
		} else {
			node = item;
		}

		// Register the template or prop
		if (isTemplate) {
			m_templates.emplace(id, std::move(node));
		} else {
			LoadProp(node, id);
		}
	}
}

SceneGraph::Model *PropDB::FindModel(std::string_view name)
{
	// FIXME: Pi::FindModel should allow zero-copy strings
	return Pi::FindModel(std::string(name));
}

const Json &PropDB::FindStyle(std::string_view name)
{
	auto iter = m_styles.find(name);
	// Try to load the style object from a JSON file.
	if (iter == m_styles.end()) {
		std::string filename = fmt::format("cockpits/styles/{}.json", name);
		Json styleObj = JsonUtils::LoadJsonDataFile(filename);

		// TODO: validation / default values for the style object

		iter = m_styles.emplace(std::string_view(name), std::move(styleObj)).first;
	}

	return iter->second;
}

void PropDB::LoadProp(const Json &node, std::string_view id)
{
	PROFILE_SCOPED()
	// Log::Verbose("PropDB::LoadProp {}\n{}\n", id, node.dump(2));

	buildingProp = new PropInfo();

	// TODO: exception / missing value safety

	buildingProp->id = id;
	buildingProp->i18n_key = node["tooltip"].get<std::string_view>();
	buildingProp->model = FindModel(node["model"]);

	// const Json &style = FindStyle(node["style"])["label"];

	// Load each label def into a LabelInfo struct
	for (const auto &pair : node["labels"].items()) {
		LabelInfo label = {};

		// TODO: convert string constants into alignment enums
		// label.halign = style.value("halign", 0);
		// label.valign = style.value("valign", 0);
		// TODO: load font from style

		std::string_view labelId = pair.key();
		if (pair.value().is_object()) {
			const Json &info = pair.value();

			label.tagName = info["tag"].get<std::string_view>();
			label.text = info["text"].get<std::string_view>();

			// TODO: convert string constants into alignment enums
			// label.halign = info.value("halign", label.halign);
			// label.valign = info.value("valign", label.valign);
			// TODO: load font from label def if present
		} else if (pair.value().is_string()) {
			std::string tagName = fmt::format("label_{}", labelId);
			label.tagName = std::string_view(tagName);
			label.text = pair.value().get<std::string_view>();
		} else {
			Log::Warning("Label definition {} in prop {} is invalid (expected: object|string)\n", labelId, id);
			continue;
		}

		buildingProp->labels.emplace_back(std::move(label));
	}

	// Load each module def into a PropModule struct
	for (const auto &pair : node["modules"].items()) {
		std::string_view moduleId = pair.key();
		const Json &info = pair.value();

		if (!info.is_object()) {
			Log::Warning("Invalid module definition {}.{} (expected: object)\n", id, moduleId);
			continue;
		}

		std::string_view type = info["type"];
		PropModule *module = nullptr;
		if (type == "ToggleSwitch") {
			module = new PMToggleSwitch();
		} else if (type == "Model") {
			module = new PMModel();
		} else {
			Log::Warning("Unknown module type {} in module {}.{}\n", type, id, moduleId);
			continue;
		}

		if (info.count("model"))
			module->model = FindModel(info["model"]);

		if (info.count("tag")) {
			module->parentTag = info["tag"].get<std::string_view>();
			if (!buildingProp->model->FindTagByName(module->parentTag)) {
				Log::Warning("Module {}.{}: no parent tag {} exists in model {}.\n",
					id, moduleId, module->parentTag, buildingProp->model->GetName());

				delete module;
				continue;
			}
		}

		module->index = buildingProp->modules.size();

		SceneGraph::Model *model = module->model ? module->model : buildingProp->model;
		module->init(this, model, moduleId, info);

		buildingProp->modules.emplace_back(module);
	}

	m_props.emplace(buildingProp->id.sv(), buildingProp);
	buildingProp = nullptr;
}

Json PropDB::MergeTemplate(const Json &node, const Json &parent)
{
	Json out = parent; // copy the parent node's contents

	// Ensure we have a labels object
	Json &labels = out["labels"];
	if (!labels.is_object())
		labels = Json::object();

	// Ensure we have a modules object
	Json &modules = out["modules"];
	if (!modules.is_object())
		modules = Json::object();

	for (const auto &pair : node.items()) {
		if (pair.key() == "labels") {
			// Append entries in the labels table to this node,
			// overwriting previous duplicate entries
			if (!pair.value().is_object())
				continue;

			for (const auto &labelPair : pair.value().items()) {
				labels[labelPair.key()] = labelPair.value();
			}

		} else if (pair.key() == "modules") {
			// Append entries in the modules table to this node,
			// overwriting previous duplicate entries
			if (!pair.value().is_object())
				continue;

			for (const auto &modulePair : pair.value().items()) {
				modules[modulePair.key()] = modulePair.value();
			}

		} else if (starts_with(pair.key(), "label:")) {
			// Patch a label definition with a text string override
			std::string labelName = pair.key().substr(6);
			if (!labels.count(labelName) || !pair.value().is_string())
				continue;

			Json &label = labels[labelName];
			if (label.is_object()) {
				label["text"] = pair.value();
			} else {
				label = pair.value();
			}

		} else if (starts_with(pair.key(), "module:")) {
			// Patch a module definition with additional data
			std::string moduleName = pair.key().substr(7);
			if (!modules.count(moduleName) || !pair.value().is_object())
				continue;

			Json &module = modules[moduleName];
			for (const auto &modulePair : pair.value().items()) {
				module[modulePair.key()] = modulePair.value();
			}

		} else {
			// Just overwrite the key as it's nothing special
			out[pair.key()] = pair.value();
		}
	}

	return out;
}

LuaRef PropDB::LoadLuaExpr(const Json &expr, bool asReturn)
{
	lua_State *l = Lua::manager->GetLuaState();
	std::string chunk;

	if (asReturn) {
		chunk = fmt::format("return {}", expr.get<std::string_view>());
	} else {
		chunk = expr.get<std::string>();
	}

	Log::Verbose("Loading lua expr:\n\t{}\n", chunk);

	luaL_loadbuffer(l, chunk.c_str(), chunk.size(), expr.get<std::string>().c_str());
	m_envTable.PushCopyToStack();
	lua_setupvalue(l, -2, 1);

	LuaRef ret(l, -1);
	lua_pop(l, 1);

	return ret;
}

void PropDB::LoadAction(const Json &node, SceneGraph::Model *model, std::string_view id, uint16_t index)
{
	if (!buildingProp) {
		Log::Warning("Cannot load a prop action without a valid prop\n");
		return;
	}

	if (buildingProp->actions.size() >= 255) {
		Log::Warning("Too many actions defined for prop {}\n", buildingProp->id.sv());
		return;
	}

	ActionInfo action = {};
	action.moduleId = uint16_t(buildingProp->modules.size()) | uint32_t(index) << 16;

	if (node.is_object()) {
		action.tagName = node["tag"].get<std::string_view>();
		action.colliderType = node["type"] == "box" ? InteractionScene::BOX_BIT : 0;
	} else {
		std::string tagName = fmt::format("tag_{}", id);
		action.tagName = std::string_view(tagName);
		action.colliderType = InteractionScene::BOX_BIT;
	}

	if (model->FindTagByName(action.tagName) == nullptr) {
		Log::Warning("Cannot find tag {} for action {} in model {}\n", action.tagName.sv(), id, model->GetName());
		return;
	}

	buildingProp->actions.emplace_back(std::move(action));
}

PropInfo *PropDB::GetProp(std::string_view name)
{
	auto iter = m_props.find(name);
	if (iter != m_props.end())
		return iter->second.get();

	return nullptr;
}

LuaRef &PropDB::GetEnvTable()
{
	return m_envTable;
}

// ============================================================================

void PropModule::callBinding(Context *ctx, LuaRef &binding, int nret)
{
	// TODO: error handling for lua binding snippets
	binding.PushCopyToStack();

	pi_lua_protected_call(ctx->lua, 0, nret);

	// call the function
	// int ok = lua_pcall(ctx->lua, 0, nret, 0);
	// if (ok != LUA_OK) {
	// 	Log::Warning("Lua Error encountered while executing module binding.\n{}\n",
	// 		lua_tostring(ctx->lua, -1));
	// 	lua_pop(ctx->lua, 1);
	// }
}
