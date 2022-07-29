// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Prop.h"

#include "CockpitScene.h"
#include "InteractionScene.h"
#include "Lang.h"

#include "lua/Lua.h"
#include "matrix3x3.h"
#include "matrix4x4.h"
#include "scenegraph/Label3D.h"
#include "scenegraph/Tag.h"
#include "scenegraph/ModelNode.h"
#include "scenegraph/NodeVisitor.h"

#include <string_view>
#include <memory>

using namespace Cockpit;

// ============================================================================

class LabelUpdateVisitor : public SceneGraph::NodeVisitor {
public:
	LabelUpdateVisitor(PropInfo *type) :
		m_type(type)
	{}

	void ApplyLabel(SceneGraph::Label3D &label) override
	{
		std::string_view tagname = label.GetName();
		Log::Info("Processing label {}\n", tagname);

		for (const auto &labelInfo : m_type->labels) {
			if (labelInfo.tagName != tagname)
				continue;

			std::string_view combinedKey = labelInfo.text.sv();

			// if the text is entirely empty, assume this label is disabled
			if (combinedKey.empty()) {
				label.SetText("");
				break;
			}

			size_t sep_idx = combinedKey.find('/');

			std::string_view i18n_resource;
			std::string_view i18n_key;

			if (sep_idx == std::string_view::npos) {
				i18n_resource = "cockpit";
				i18n_key = combinedKey;
			} else {
				i18n_resource = combinedKey.substr(0, sep_idx);
				i18n_key = combinedKey.substr(sep_idx + 1);
			}

			Log::Info("i18n_resource: {}, i18n_key: {}\n", i18n_resource, i18n_key);

			// TODO: pull localization region from user settings
			Lang::Resource &res = Lang::GetResource(i18n_resource, "en");
			Log::Info("Label text: {}\n", res.Get(i18n_key));

			label.SetText(res.Get(i18n_key));
			break;
		}
	}

	PropInfo *m_type;
};

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

	// Setup labels for this prop
	LabelUpdateVisitor visitor(m_propInfo);
	m_modelInstance->GetRoot()->Accept(visitor);

	// Create model instances and state variables for this prop's modules
	for (const auto &module : m_propInfo->modules) {
		ModuleState state = {};
		state.state = module->createState();

		if (module->model) {
			// Create a new instance of this module's model and parent it to the specified tag
			state.modelInstance.reset(module->model->MakeInstance());

			if (module->parentTag)
				state.parentTag = m_modelInstance->FindTagByName(module->parentTag);

			SceneGraph::Group *root = state.parentTag ?
				state.parentTag : m_modelInstance->GetRoot().Get();

			root->AddChild(new SceneGraph::ModelNode(state.modelInstance.get()));
		}

		m_moduleCtx.emplace_back(std::move(state));
	}

	// Setup interaction triggers in scene space
	for (size_t idx = 0; idx < m_propInfo->actions.size(); idx++) {
		ActionInfo &action = m_propInfo->actions[idx];

		// 24.8 prop ID : actionId pair
		uint32_t triggerId = (propId << 8) | (idx & 0xFF);

		CreateTrigger(action, action.moduleId & 0xFFFF, triggerId);
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

		PropModule::Context ctx = SetupContext(m_moduleCtx[idx]);
		module->updateState(&ctx, delta);
	}
}

void Prop::Render(Graphics::Renderer *r, const matrix4x4f &viewTransform)
{
	m_modelInstance->Render(viewTransform * matrix4x4f(m_orient, m_pos));
}

void Prop::UpdateTriggers()
{
	PROFILE_SCOPED()

	// Update all trigger positions with this module's transform.
	for (size_t idx = 0; idx < m_propInfo->actions.size(); idx++) {
		ActionState &action = m_actionTriggers[idx];
		ModuleState &state = m_moduleCtx[action.moduleId];

		UpdateTrigger(action, state);
	}
}

void Prop::UpdateTrigger(PropModule *module, uint16_t index)
{
	PROFILE_SCOPED()

	// Update a specific trigger registered by the associated module
	for (size_t idx = 0; idx < m_propInfo->actions.size(); idx++) {
		ActionState &action = m_actionTriggers[idx];
		if (action.moduleId != module->index)
			continue;

		ModuleState &state = m_moduleCtx[module->index];
		UpdateTrigger(action, state);
		break;
	}
}

// Propagate an ActionPressed event to the module which registered the action trigger
bool Prop::OnActionPressed(uint32_t action)
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

	PropModule::Context ctx = SetupContext(m_moduleCtx[moduleIdx]);
	return m_propInfo->modules[moduleIdx]->onActionPressed(&ctx, actionIdx);
}

// ============================================================================

// Create an interaction trigger from an ActionInfo and add it to the InteractionScene
void Prop::CreateTrigger(const ActionInfo &action, uint32_t moduleId, uint32_t actionId)
{
	const ModuleState &state = m_moduleCtx[moduleId];
	InteractionScene *iScene = m_cockpit->GetInteraction();

	ActionState outState = { InteractionScene::INVALID_ID, moduleId, nullptr };

	// The trigger is defined as a tag on the module's model
	if (state.modelInstance)
		outState.actionTag = state.modelInstance->FindTagByName(action.tagName);
	else // The trigger is defined as a tag on the parent model
		outState.actionTag = m_modelInstance->FindTagByName(action.tagName);

	assert(outState.actionTag);

	// Calculate the transform for the resulting trigger
	matrix4x4f transform = GetModuleTagTransform(state, outState.actionTag);

	if (action.colliderType & InteractionScene::BOX_BIT) {
		// Calculate extents from 3x3 combined rotation-scale matrix
		// We make the assumption that non-uniform scale is applied first.
		matrix3x3f rotScale = outState.actionTag->GetTransform().GetOrient();
		vector3f extents(
			rotScale.VectorX().Length(),
			rotScale.VectorY().Length(),
			rotScale.VectorZ().Length());

		// Ensure we normalize the orient to only rotation
		outState.actionTrigger = iScene->AddBoxTrigger(actionId,
			transform.GetTranslate(),
			transform.GetOrient().Normalized(),
			extents);
	} else {
		// Calculate scale from uniform matrix scale in world space.
		matrix3x3f rotScale = outState.actionTag->GetTransform().GetOrient();
		float scale = rotScale.VectorX().Length();

		outState.actionTrigger = iScene->AddSphereTrigger(actionId,
			transform.GetTranslate(), scale);
	}

	m_actionTriggers.emplace_back(std::move(outState));
}

void Prop::UpdateTrigger(const ActionState &action, const ModuleState &state)
{
	// Update the global transform of the parent tag
	if (state.parentTag)
		state.parentTag->UpdateGlobalTransform();

	// Update the global transform of the action tag
	action.actionTag->UpdateGlobalTransform();

	// Calculate the updated cockpit-space transform of this trigger
	matrix4x4f transform = matrix4x4f(m_orient, m_pos) *
		GetModuleTagTransform(state, action.actionTag);

	m_cockpit->GetInteraction()->UpdateTriggerPos(action.actionTrigger,
		transform.GetTranslate(), transform.GetOrient().Normalized());
}

// Calculate the top-level "cockpit space" transform for the given module's tag
matrix4x4f Prop::GetModuleTagTransform(const ModuleState &state, const SceneGraph::Tag *actionTag)
{
	matrix4x4f transform = actionTag->GetGlobalTransform();

	if (state.parentTag)
		transform = state.parentTag->GetGlobalTransform() * transform;

	return transform;
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
PropModule::Context Prop::SetupContext(ModuleState &module)
{
	PropModule::Context ctx = {};

	ctx.prop = this;
	ctx.model = module.modelInstance.get();
	if (!ctx.model)
		ctx.model = m_modelInstance.get();

	ctx.state = module.state;
	ctx.cockpit = m_cockpit;
	ctx.lua = m_env.GetLua();

	return ctx;
}
