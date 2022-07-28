// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "PropDB.h"

#include "RefCounted.h"
#include "lua/LuaRef.h"
#include "matrix3x3.h"
#include "matrix4x4.h"
#include "vector3.h"

#include <map>
#include <memory>
#include <string_view>

namespace Graphics {
	class Renderer;
}

namespace SceneGraph {
	class Model;
	class Tag;
}

namespace Cockpit {
	class CockpitScene;
	struct PropInfo;

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
		void Render(Graphics::Renderer *r, const matrix4x4f &viewTransform);

		// Update the position and transforms of all triggers registered by this prop
		void UpdateTriggers();

		// Update a specific module's trigger position and transform
		void UpdateTrigger(PropModule *module, uint16_t index);

		// Called when a trigger registered by this prop is activated
		bool OnActionPressed(uint32_t action);
		void OnActionDragged(uint32_t action, vector2f delta);
		void OnActionReleased(uint32_t action);

	private:
		struct ModuleState {
			void *state;
			std::unique_ptr<SceneGraph::Model> modelInstance;
			SceneGraph::Tag *parentTag;
		};

		struct ActionState {
			uint32_t actionTrigger;
			uint32_t moduleId; // indirection to allow quick lookup of the owning module
			SceneGraph::Tag *actionTag;
		};

		void CreateTrigger(const ActionInfo &action, uint32_t moduleId, uint32_t actionId);
		void UpdateTrigger(const ActionState &action, const ModuleState &state);
		matrix4x4f GetModuleTagTransform(const ModuleState &state, const SceneGraph::Tag *actionTag);

		void SetupEnvironment();

		PropModule::Context SetupContext(ModuleState &modState);

		std::unique_ptr<SceneGraph::Model> m_modelInstance;

		vector3f m_pos;
		matrix3x3f m_orient;

		CockpitScene *m_cockpit;
		PropInfo *m_propInfo;
		LuaRef m_instance;
		LuaRef &m_env;

		std::vector<ModuleState> m_moduleCtx;
		std::vector<ActionState> m_actionTriggers;
	};
}
