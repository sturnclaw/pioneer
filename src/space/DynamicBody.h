
#pragma once

#include "Manager.h"

/*
	Total-frame order:

	TOP_OF_FRAME:
	- ClearForces

	Order independent:
	- CalcDynamicForce
	  +- CalcExternalForces
	  +- CalcAtmoForce
	- CalcShipAtmoForce

	BOTTOM_OF_FRAME:
	- UpdateDynamic

*/

namespace PhysicsSpace {

	class ClearForces;
	class CalcDynamicForce;
	class DynamicUpdateSystem;

} // namespace PhysicsSpace
