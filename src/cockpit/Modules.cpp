// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Modules.h"

#include "JsonUtils.h"
#include "lua/Lua.h"
#include "lua/LuaTable.h"
#include "lua/LuaUtils.h"
#include "scenegraph/Animation.h"

using namespace Cockpit;

// ============================================================================

void PMToggleSwitch::init(PropDB *db, SceneGraph::Model *model, const Json &node)
{
	stateBinding = db->LoadLuaExpr(node["state"], true);
	actionBinding = db->LoadLuaExpr(node["action"]);

	const std::string &animName = node["animation"];

	SceneGraph::Animation *anim = model->FindAnimation(animName);
	if (anim != nullptr) {
		anim_idx = model->FindAnimationIndex(anim);
	} else {
		Log::Warning("Could not find animation {} in model {}\n", animName, model->GetName());
	}

	// TODO: load and register action binding for this prop module
	db->LoadAction(node["trigger"], 0);
}

void PMToggleSwitch::updateState(Context *ctx, float delta)
{
	callBinding(ctx, stateBinding, 1);
	bool newState = LuaPull<bool>(ctx->lua, -1);
	lua_pop(ctx->lua, 1);

	if (newState != *getState(ctx)) {
		toggleState(ctx);
	}

	if (!ctx->model->GetAnimationActive(anim_idx))
		return;

	SceneGraph::Animation *anim = ctx->model->GetAnimations()[anim_idx];

	float incr = newState ? delta : -delta;
	double newProgress = anim->GetProgress() + (incr / anim->GetDuration());

	anim->SetProgress(Clamp(newProgress, 0.0, 1.0));
	anim->Interpolate();

	if (anim->GetProgress() == 0.0 || anim->GetProgress() == 1.0) {
		ctx->model->SetAnimationActive(anim_idx, false);
	}
}

bool PMToggleSwitch::onActionPressed(Context *ctx, uint32_t actionIdx)
{
	toggleState(ctx);
	callBinding(ctx, actionBinding, 0);

	return false;
}

void PMToggleSwitch::toggleState(Context *ctx)
{
	*getState(ctx) = !*getState(ctx);
	ctx->model->SetAnimationActive(anim_idx, true);
}
