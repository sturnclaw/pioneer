// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Modules.h"

#include "Prop.h"
#include "JsonUtils.h"
#include "lua/Lua.h"
#include "lua/LuaPushPull.h"
#include "lua/LuaUtils.h"
#include "scenegraph/Animation.h"
#include "scenegraph/Model.h"

using namespace Cockpit;

void PMModel::init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node)
{
	// Intentionally left blank
}

void PMModel::updateState(Context *ctx, float delta)
{
	// Intentionally left blank
}

// ============================================================================

void PMToggleSwitch::init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node)
{
	if (node.count("state") && node.count("action")) {
		stateBinding = db->LoadLuaExpr(node["state"], true);
		actionBinding = db->LoadLuaExpr(node["action"]);
	}

	const std::string_view animName = node.value("animation", id);

	SceneGraph::Animation *anim = model->FindAnimation(animName);
	if (anim != nullptr) {
		anim_idx = model->FindAnimationIndex(anim);
	} else {
		Log::Warning("Could not find animation {} in model {}\n", animName, model->GetName());
	}

	// TODO: load and register action binding for this prop module
	db->LoadAction(node.value("trigger", Json(nullptr)), model, id, 0);
}

void PMToggleSwitch::updateState(Context *ctx, float delta)
{
	bool curState = *getState(ctx);
	if (stateBinding.IsValid()) {
		callBinding(ctx, stateBinding, 1);
		bool newState = LuaPull<bool>(ctx->lua, -1);
		lua_pop(ctx->lua, 1);

		if (newState != curState) {
			curState = newState;
			toggleState(ctx);
		}
	}

	if (!ctx->model->GetAnimationActive(anim_idx))
		return;

	SceneGraph::Animation *anim = ctx->model->GetAnimations()[anim_idx];

	float incr = curState ? delta : -delta;
	double newProgress = anim->GetProgress() + (incr / anim->GetDuration());

	anim->SetProgress(Clamp(newProgress, 0.0, 1.0));
	anim->Interpolate();

	// Update our trigger in case it is being animated
	ctx->prop->UpdateTrigger(this, 0);
	// TODO: mark all triggers in the prop dirty in case this module is used to animate other modules.
	// This should be the job of a dedicated PMAnimator module
	ctx->prop->MarkTriggersDirty();

	if (anim->GetProgress() == 0.0 || anim->GetProgress() == 1.0) {
		ctx->model->SetAnimationActive(anim_idx, false);
	}
}

bool PMToggleSwitch::onActionPressed(Context *ctx, uint32_t actionIdx)
{
	toggleState(ctx);

	if (actionBinding.IsValid())
		callBinding(ctx, actionBinding, 0);

	return false;
}

void PMToggleSwitch::toggleState(Context *ctx)
{
	*getState(ctx) = !*getState(ctx);
	ctx->model->SetAnimationActive(anim_idx, true);
}
