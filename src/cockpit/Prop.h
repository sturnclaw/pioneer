// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "RefCounted.h"
#include "core/StringName.h"
#include "lua/LuaRef.h"
#include "scenegraph/MatrixTransform.h"
#include "scenegraph/Model.h"
#include "matrix3x3.h"
#include "vector3.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace Cockpit {
	class CockpitScene;
	struct PropInfo;
	class Prop;

	class PropDB {
	public:
		void LoadPropCtx();

		void LoadProps(const std::string &path);

		PropInfo *GetProp(std::string_view name);

		LuaRef &GetEnvTable();

		LuaRef LoadLuaExpr(const Json &expr, bool asReturn = false);

		void LoadAction(const Json &node, SceneGraph::Model *model, std::string_view id, uint16_t index);

	private:
		SceneGraph::Model *FindModel(std::string_view name);
		const Json &FindStyle(std::string_view name);

		void LoadProp(const Json &node, std::string_view id);
		Json MergeTemplate(const Json &node, const Json &parent);

	private:
		std::map<std::string_view, std::unique_ptr<PropInfo>> m_props;
		std::map<std::string, Json, std::less<>> m_templates;
		std::map<std::string, Json, std::less<>> m_styles;

		LuaRef m_envTable;
		PropInfo *buildingProp;
	};

	// A PropModule instance is created when used in a Prop definition, and
	// operates on Context* which contain all per-instance state for that module.
	struct PropModule {
		struct Context {
			Prop *prop;
			SceneGraph::Model *model;
			CockpitScene *cockpit;
			lua_State *lua;
			void *state;
		};

		SceneGraph::Model *model;
		StringName parentTag;
		uint32_t index;

		PropModule() = default;
		virtual ~PropModule() = default;

		void callBinding(Context *ctx, LuaRef &binding, int nret = 0);

		virtual void init(PropDB *db, SceneGraph::Model *model, std::string_view id, const Json &node) = 0;

		virtual void *createState() = 0;
		virtual void deleteState(void *state) = 0;

		virtual void updateState(Context *ctx, float delta) = 0;

		// Action handling is simple:
		// - single click interactions are handled by returning false from
		// onActionPressed(). Mouse drag / release events will not be captured.
		//
		// - ongoing user interaction is handled by returning true from
		// onActionPressed(). Mouse inputs will be captured until the button is
		// released and drag and release events are forwarded to this module;
		// e.g. for sliders or complex screen interaction.

		// Trigged when the user clicks the mouse button on an action trigger
		// registered by this module.
		virtual bool onActionPressed(Context *ctx, uint32_t actionIdx) { return false; }

		// Triggered when the user drags the mouse. delta is the mouse delta in screen space.
		virtual void onActionDragged(Context *ctx, uint32_t actionIdx, vector2f delta) {}

		// Triggers when the user releases the mouse button
		virtual void onActionReleased(Context *ctx, uint32_t actionIdx) {}
	};

	struct LabelInfo {
		StringName tagName;
		StringName text;
		int valign;
		int halign;
		// RefCountedPtr<Text::DistanceFieldFont> font;
	};

	struct ActionInfo {
		// Name of the tag this action trigger is associated with
		StringName tagName;
		// InteractionScene::BOX_BIT indicates an AABB trigger;
		// otherwise this is a sphere trigger
		uint32_t colliderType;
		// 16.16 pair identifying which module this action belongs to and
		// which action in the module it is
		uint32_t moduleId;
	};

	// PropInfo represents static type information about a given prop
	// in the "pre-instance" state. It owns all PropModules and associated
	// information about the prop type.
	struct PropInfo {
		StringName id;
		StringName i18n_key;
		SceneGraph::Model *model;

		std::vector<std::unique_ptr<PropModule>> modules;
		std::vector<LabelInfo> labels;
		std::vector<ActionInfo> actions;

		LuaRef onCreate;
	};

	// Prop represents a "reified instance" of a given PropInfo as used in a
	// visible cockpit instance. It stores all runtime state about the prop, as
	// well as model instances for animation, etc.
	class Prop {
	public:
		Prop(PropInfo *type, CockpitScene *cockpit, uint32_t propId, LuaRef &luaEnv);
		~Prop();

		SceneGraph::Model *GetModel() { return m_modelInstance.get(); }

		matrix3x3f GetOrient() const { return m_orient; }
		vector3f GetPosition() const { return m_pos; }

		void SetOrient(const matrix3x3f &orient);
		void SetPosition(const vector3f &pos);

		const PropInfo *GetPropInfo() const { return m_propInfo; }

		void Update(float delta);
		void UpdateTriggers();

		// Update a specific module's trigger
		void UpdateTrigger(PropModule *module, uint16_t index);

		void Render(Graphics::Renderer *r, const matrix4x4f &viewTransform);

		bool TriggerAction(uint32_t action);
		void OnActionDragged(uint32_t action, vector2f delta);
		void OnActionReleased(uint32_t action);

	private:
		struct ModuleState {
			void *state;
			std::unique_ptr<SceneGraph::Model> modelInstance;
			SceneGraph::MatrixTransform *parentTag;
		};

		};

		void CreateTrigger(const ActionInfo &action, uint32_t actionId);
		matrix4x4f GetModuleTagTransform(PropModule *module, SceneGraph::Model *modelInstance, std::string_view tagName);

		void SetupEnvironment();

		PropModule::Context SetupContext(uint32_t moduleIdx);

		std::unique_ptr<SceneGraph::Model> m_modelInstance;

		vector3f m_pos;
		matrix3x3f m_orient;

		CockpitScene *m_cockpit;
		PropInfo *m_propInfo;
		LuaRef m_instance;
		LuaRef &m_env;

		std::vector<ModuleState> m_moduleCtx;
		std::vector<uint32_t> m_actionTriggers;
	};
}
