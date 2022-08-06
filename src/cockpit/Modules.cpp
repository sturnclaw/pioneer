// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Modules.h"

#include "FloatComparison.h"
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

void PMAnimator::init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node)
{
	if (node.count("state")) {
		stateBinding = db->LoadLuaExpr(node["state"], true);
	}

	const std::string_view animName = node.value("animation", id);
	SceneGraph::Animation *anim = model->FindAnimation(animName);

	if (anim != nullptr) {
		anim_idx = model->FindAnimationIndex(anim);
	} else {
		Log::Warning("Could not find animation {} in model {}\n", animName, model->GetName());
	}
}

void PMAnimator::updateState(Context *ctx, float delta)
{
	float curProgress = *getState(ctx);
	float newProgress = curProgress;

	SceneGraph::Animation *anim = ctx->model->GetAnimations()[anim_idx];

	if (stateBinding.IsValid()) {
		callBinding(ctx, stateBinding, 1);

		if (lua_isnumber(ctx->lua, -1)) {
			// if we're passed a number, just animate to that progress directly
			newProgress = LuaPull<float>(ctx->lua, -1);
		} else {
			// otherwise play the animation in forward or reverse
			bool active = LuaPull<bool>(ctx->lua, -1);
			float incr = active ? delta : -delta;
			newProgress += incr / anim->GetDuration();
		}

		newProgress = Clamp(newProgress, 0.f, 1.f);
		*getState(ctx) = newProgress;

		lua_pop(ctx->lua, 1);
	}

	if (!is_equal_general(curProgress, newProgress)) {
		anim->SetProgress(newProgress);
		anim->Interpolate();

		ctx->prop->MarkTriggersDirty();
	}
}

// ============================================================================

void PMButton::init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node)
{
	if (node.count("action")) {
		actionBinding = db->LoadLuaExpr(node["action"]);
	}
	const std::string_view animName = node.value("animation", id);

	SceneGraph::Animation *anim = model->FindAnimation(animName);
	if (anim != nullptr) {
		anim_idx = model->FindAnimationIndex(anim);
	} else {
		Log::Warning("Could not find animation {} in model {}\n", animName, model->GetName());
	}

	db->LoadAction(node.value("trigger", Json(nullptr)), model, id, 0);
}

void PMButton::updateState(Context *ctx, float delta)
{
	if (anim_idx < 0 || !ctx->model->GetAnimationActive(anim_idx))
		return;

	SceneGraph::Animation *anim = ctx->model->GetAnimations()[anim_idx];
	float animProgress = anim->GetProgress();
	animProgress += delta / anim->GetDuration();

	anim->SetProgress(Clamp(animProgress, 0.f, 1.f));
	anim->Interpolate();

	// Update our trigger in case it is being animated
	ctx->prop->UpdateTrigger(this, 0);

	if (anim->GetProgress() == 1.0) {
		ctx->model->SetAnimationActive(anim_idx, false);
	}
}

bool PMButton::onActionPressed(Context *ctx, uint32_t actionIdx)
{
	if (actionBinding.IsValid()) {
		callBinding(ctx, actionBinding);
	}

	if (anim_idx < 0)
		return false;

	// if we're clicked, set animation progress to 0 and start animating
	ctx->model->SetAnimationActive(anim_idx, true);
	ctx->model->GetAnimations()[anim_idx]->SetProgress(0.0);

	return false;
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
