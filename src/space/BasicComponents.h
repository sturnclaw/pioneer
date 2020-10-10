
#pragma once

#include "FrameId.h"
#include "matrix4x4.h"
#include "vector3.h"

namespace PhysicsSpace {

	struct Transform {
		vector3d position;
		matrix3x3d orient;
		FrameId frame;
	};

	// Represents core data about any movable body.
	struct DynamicBody {
		vector3d velocity;
		vector3d angVelocity;

		double mass;
		double massRadius; // set in a mickey-mouse fashion from the collision mesh and used to calculate m_angInertia
		double angInertia; // always sphere mass distribution
	};

	// forward declared for mocking purposes
	struct PlanetData {};
	void GetAtmosphericState(const PlanetData &pd, double height, double *outPressure, double *outDensity);

	// Information about basic spherical-drag approximation
	// Used on bodies without better drag data as a last-resort
	struct BasicDragData {
		double area;
		double dragCoeff;
	};

	// Cache forces involved in physics calculations of bodies
	struct ForceCache {
		vector3d force;
		vector3d torque;
		vector3d externalForce;
		vector3d atmoForce;
	};

	// for mocking purposes....
	struct Frame {
		bool IsRotFrame() const;
		double GetAngSpeed() const;
		uint32_t bodyId;
		double radius;

		// more members as necessary for proof-of-concept
	};

} // namespace PhysicsSpace
