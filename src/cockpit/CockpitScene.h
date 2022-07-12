// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "JsonFwd.h"
#include "Quaternion.h"
#include "vector3.h"

#include <map>
#include <memory>
#include <vector>

namespace Graphics {
	class Renderer;
	class Material;
}

namespace SceneGraph {
	class Model;
}

class Camera;
class Ship;
struct ShipType;

namespace Cockpit {

	// Forward Declarations

	class DrawingContext;
	class InteractionScene;

	class PropInfo;

	class CockpitScene {
	public:
		enum DebugFlags : uint32_t {
			DEBUG_NONE = 0,
			DEBUG_SHOW_TRIGGERS = 1 << 0,
			DEBUG_SHOW_DISPLAYS = 1 << 1
		};

	public:
		CockpitScene(Graphics::Renderer *r);
		~CockpitScene();

		void InitForShipType(const ShipType *type);
		const ShipType *GetShipType() const { return m_shipType; }

		void Load(const std::string &cockpitPath);
		void Clear();

		void SetShip(const Ship *ship);

		void SetDebugFlags(uint32_t flags);

		void Update(matrix3x3d viewOrient, vector3d viewOffset);

		void Render(Graphics::Renderer *r, Camera *camera, const matrix4x4f &viewTransform);

	private:
		void LoadProps(const Json &node);

	private:
		std::unique_ptr<InteractionScene> m_interactionScene;
		std::unique_ptr<SceneGraph::Model> m_model;

		std::unique_ptr<Graphics::Material> m_debugMat;

		vector3f m_camPosition;
		matrix3x3f m_camOrient;

		const Ship *m_ship;
		const ShipType *m_shipType;

		vector3f m_lastTrace;

		// std::vector<std::unique_ptr<DrawingContext>> m_displayContexts;
		// std::vector<std::unique_ptr<PropInfo>> m_props;
		std::map<size_t, PropInfo*> m_actionMap;
	};
}
