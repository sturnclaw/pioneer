// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Prop.h"

#include "CockpitScene.h"
#include "InteractionScene.h"
#include "JsonUtils.h"
#include "Modules.h"
#include "Pi.h"

#include "lua/Lua.h"
#include "lua/LuaTable.h"
#include "lua/LuaUtils.h"
#include "matrix3x3.h"
#include "matrix4x4.h"
#include "scenegraph/MatrixTransform.h"
#include "scenegraph/ModelNode.h"

#include <string_view>
#include <memory>

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
			m_templates.emplace(std::string(id), std::move(node));
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

		iter = m_styles.emplace(name, std::move(styleObj)).first;
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

	const Json &style = FindStyle(node["style"])["label"];

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
			label.tagName = labelId;
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
		} else {
			Log::Warning("Unknown module type {} in module {}.{}\n", type, id, moduleId);
			continue;
		}

		if (info.count("model"))
			module->model = FindModel(info["model"]);
		if (info.count("tag"))
			module->parentTag = info["tag"].get<std::string_view>();

		SceneGraph::Model *model = module->model ? module->model : buildingProp->model;
		module->init(this, model, info);

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

void PropDB::LoadAction(const Json &node, uint16_t id)
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

	action.tagName = node["tag"].get<std::string_view>();
	action.colliderType = node["type"] == "box" ? InteractionScene::BOX_BIT : 0;
	action.moduleId = uint16_t(buildingProp->modules.size()) | uint32_t(id) << 16;

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

// ============================================================================

Prop::Prop(PropInfo *type, CockpitScene *cockpit, uint32_t propId, LuaRef &luaEnv) :
	m_cockpit(cockpit),
	m_propInfo(type),
	m_env(luaEnv)
{
	PROFILE_SCOPED()

	lua_State *l = m_env.GetLua();

	// Create the instance table for this prop
	lua_newtable(l);
	m_instance = LuaRef(l, -1);
	lua_pop(l, 1);

	// Call the onCreate binding if present in the PropInfo
	if (m_propInfo->onCreate.IsValid()) {
		SetupEnvironment();

		m_propInfo->onCreate.PushCopyToStack();
		lua_pcall(l, 0, 0, 0);
	}

	// Create the model instance for this prop
	m_modelInstance.reset(m_propInfo->model->MakeInstance());

	// Create model instances and state variables for this prop's modules
	for (const auto &module : m_propInfo->modules) {
		PropState state = {};
		state.state = module->createState();

		if (module->model) {
			// Create a new instance of this module's model and parent it to the specified tag
			state.modelInstance.reset(module->model->MakeInstance());

			SceneGraph::Group *rootNode = m_modelInstance->FindTagByName(module->parentTag);
			if (!rootNode)
				rootNode = m_modelInstance->GetRoot().Get();

			rootNode->AddChild(new SceneGraph::ModelNode(state.modelInstance.get()));
		}

		m_moduleCtx.emplace_back(std::move(state));
	}

	// Setup interaction triggers in scene space
	for (size_t idx = 0; idx < m_propInfo->actions.size(); idx++) {
		ActionInfo &action = m_propInfo->actions[idx];

		// 24.8 prop ID : actionId pair
		uint32_t triggerId = (propId << 8) | (idx & 0xFF);

		CreateTrigger(action, triggerId);
	}
}

Prop::~Prop()
{
	// Cleanup module states and model instances
	for (size_t idx = 0; idx < m_moduleCtx.size(); idx++) {
		m_propInfo->modules[idx]->deleteState(m_moduleCtx[idx].state);
		m_moduleCtx[idx].modelInstance.reset();
	}

	m_moduleCtx.clear();
	m_modelInstance.reset();
	m_instance.Unref();
}

void Prop::SetOrient(const matrix3x3f &orient)
{
	m_orient = orient;
}

void Prop::SetPosition(const vector3f &pos)
{
	m_pos = pos;
}

void Prop::Update(float delta)
{
	PROFILE_SCOPED()

	SetupEnvironment();

	for (size_t idx = 0; idx < m_moduleCtx.size(); idx++) {
		PropModule *module = m_propInfo->modules[idx].get();

		PropModule::Context ctx = SetupContext(idx);
		module->updateState(&ctx, delta);
	}
}

void Prop::UpdateTriggers()
{
	// Update all trigger positions with this module's transform.
	for (size_t idx = 0; idx < m_propInfo->actions.size(); idx++) {
		ActionInfo &action = m_propInfo->actions[idx];
		PropModule *module = m_propInfo->modules[action.moduleId & 0xFFFF].get();

		matrix4x4f transform = matrix4x4f(m_orient, m_pos) *
			GetModuleTagTransform(module, action.tagName);

		m_cockpit->GetInteraction()->UpdateTriggerPos(m_actionTriggers[idx],
			transform.GetTranslate(),
			transform.GetOrient().Normalized());
	}
}

void Prop::Render(Graphics::Renderer *r, const matrix4x4f &viewTransform)
{
	m_modelInstance->Render(viewTransform * matrix4x4f(m_orient, m_pos));
}

// Propagate an ActionPressed event to the module which registered the action trigger
bool Prop::TriggerAction(uint32_t action)
{
	PROFILE_SCOPED()

	if (action >= m_propInfo->actions.size()) {
		Log::Warning("Invalid action index {} for prop {}\n", action, m_propInfo->id.sv());
		return false;
	}

	ActionInfo &info = m_propInfo->actions[action];
	uint32_t moduleIdx = info.moduleId & 0xFFFF;
	uint32_t actionIdx = info.moduleId >> 16;

	SetupEnvironment();

	PropModule::Context ctx = SetupContext(moduleIdx);
	return m_propInfo->modules[moduleIdx]->onActionPressed(&ctx, actionIdx);
}

// Create an interaction trigger from an ActionInfo and add it to the InteractionScene
void Prop::CreateTrigger(const ActionInfo &action, uint32_t actionId)
{
	InteractionScene *iScene = m_cockpit->GetInteraction();

	uint32_t moduleId = action.moduleId & 0xFFFF;
	PropModule *module = m_propInfo->modules[moduleId].get();

	// Calculate the transform for the resulting trigger
	matrix4x4f transform = GetModuleTagTransform(module, action.tagName);

	SceneGraph::MatrixTransform *tag;
	if (module->model) {
		// The trigger is defined as a tag on the module's model
		tag = module->model->FindTagByName(action.tagName);
	} else {
		// The trigger is defined as a tag on the parent model
		tag = m_modelInstance->FindTagByName(action.tagName);
	}

	uint32_t triggerId = InteractionScene::INVALID_ID;
	if (action.colliderType & InteractionScene::BOX_BIT) {
		// Calculate extents from 3x3 combined rotation-scale matrix
		// We make the assumption that non-uniform scale is applied first,
		// and the tag is not scaled further.
		matrix3x3f rotScale = tag->GetTransform().GetOrient();
		vector3f extents(
			rotScale.VectorX().Length(),
			rotScale.VectorY().Length(),
			rotScale.VectorZ().Length());
		rotScale.Renormalize();
		extents = extents * rotScale; // transform extents into AABB space

		// Ensure we normalize the orient to only rotation
		triggerId = iScene->AddBoxTrigger(actionId,
			transform.GetTranslate(),
			transform.GetOrient().Normalized(),
			extents);
	} else {
		// Calculate scale from uniform matrix scale in world space.
		matrix3x3f rotScale = tag->GetTransform().GetOrient();
		float scale = rotScale.VectorX().Length();
		triggerId = iScene->AddSphereTrigger(actionId, transform.GetTranslate(), scale);
	}

	m_actionTriggers.push_back(triggerId);
}

matrix4x4f Prop::GetModuleTagTransform(PropModule *module, std::string_view tagName)
{
	matrix4x4f transform = matrix4x4fIdentity;
	SceneGraph::MatrixTransform *tag = nullptr;

	if (module->model) {
		SceneGraph::MatrixTransform *parentTag = m_modelInstance->FindTagByName(module->parentTag);
		if (parentTag)
			transform = parentTag->GetTransform();

		tag = module->model->FindTagByName(tagName);
	} else {
		tag = m_modelInstance->FindTagByName(tagName);
	}

	return tag ? transform * tag->GetTransform() : transform;
}

// Set the self reference in the environment table
void Prop::SetupEnvironment()
{
	m_env.PushCopyToStack();
	m_instance.PushCopyToStack();
	lua_setfield(m_env.GetLua(), -2, "self");
	lua_pop(m_env.GetLua(), 1);
}

// Populate a PropModule context for the given module
PropModule::Context Prop::SetupContext(uint32_t moduleIdx)
{
	PropModule::Context ctx = {};

	ctx.model = m_moduleCtx[moduleIdx].modelInstance.get();
	if (!ctx.model)
		ctx.model = m_modelInstance.get();

	ctx.state = m_moduleCtx[moduleIdx].state;
	ctx.cockpit = m_cockpit;
	ctx.lua = m_env.GetLua();

	return ctx;
}
