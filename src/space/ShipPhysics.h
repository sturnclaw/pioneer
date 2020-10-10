
#pragma once

#include "vector3.h"

namespace PhysicsSpace {

	struct ShipDragData {
		vector3d crossSection;
		vector3d dragCoeff;
		double liftCoeff;
		double atmoStability;
	};

	class ShipAtmoForceSystem;

} // namespace PhysicsSpace
