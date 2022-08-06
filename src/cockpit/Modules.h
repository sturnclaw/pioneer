// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "PropDB.h"
#include "core/StringName.h"
#include "lua/LuaRef.h"

#define PROP_DEFINE_STATE_TYPE(type)							\
	using StateT = type;										\
	void *createState() override								\
	{															\
		return new StateT();									\
	}															\
	void deleteState(void *state) override						\
	{															\
		delete static_cast<StateT *>(state);					\
	}															\
	StateT *getState(Context *ctx)			 					\
	{ 															\
		return static_cast<StateT *>(ctx->state);				\
	}
#define PROP_DEFINE_STATELESS()									\
	void *createState() override { return nullptr; }			\
	void deleteState(void *state) override {}					\

namespace Cockpit {

	struct PMModel : PropModule {
		PROP_DEFINE_STATELESS();

		void init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node) override;
		void updateState(Context *ctx, float delta) override;
	};

	struct PMAnimator : PropModule {
		PROP_DEFINE_STATE_TYPE(float);

		LuaRef stateBinding;

		uint32_t anim_idx = 0;

		void init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node) override;
		void updateState(Context *ctx, float delta) override;
	};

	struct PMButton : PropModule {
		PROP_DEFINE_STATELESS();

		LuaRef actionBinding;

		int32_t anim_idx = -1;

		void init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node) override;
		void updateState(Context *ctx, float delta) override;
		bool onActionPressed(Context *ctx, uint32_t actionIdx) override;
	};

	struct PMToggleSwitch : PropModule {
		PROP_DEFINE_STATE_TYPE(bool);

		PMToggleSwitch() = default;
		~PMToggleSwitch() = default;

		LuaRef stateBinding;
		LuaRef actionBinding;

		uint32_t anim_idx = 0;

		void init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node) override;

		void updateState(Context *ctx, float delta) override;

		bool onActionPressed(Context *ctx, uint32_t actionIdx) override;

		void toggleState(Context *ctx);
	};

} // namespace Cockpit
