// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "Quaternion.h"
#include "vector3.h"

#include <vector>

// Forward declarations

namespace Graphics {
	class Renderer;
	class Material;
}

struct AABBf {
	vector3f center, extents;

	// Adapted from https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
	// Performs a ray intersection test for a ray origin in AABB space with the
	// AABB center at the origin.
	// Calling code should subtract the AABB center from the origin and rotate
	// both the origin and direction into AABB space.
	bool IntersectsRay(const vector3f &o, const vector3f &d, float &out_t) const
	{
		vector3f invdir = 1.0f / d;

		vector3f t1 = (-extents - o) * invdir;
		vector3f t2 = ( extents - o) * invdir;

		vector3f tmin = vector3f(std::min(t1.x, t2.x), std::min(t1.y, t2.y), std::min(t1.z, t2.z));
		vector3f tmax = vector3f(std::max(t1.x, t2.x), std::max(t1.y, t2.y), std::max(t1.z, t2.z));

		float tN = std::max(std::max(tmin.x, tmin.y), tmin.z);
		float tF = std::min(std::min(tmax.x, tmax.y), tmax.z);

		out_t = tN;
		return tN <= tF;
	}
};

namespace Cockpit {

	// Represent an OBB by an AABB and a rotation which transforms input data
	// into the axis-aligned space of the AABB.
	struct BoxCollider {
		AABBf aabb;
		Quaternionf rotation;
		uint32_t action;

		BoxCollider(const vector3f &center, const vector3f &extents, const matrix3x3f &orient)
		{
			rotation = Quaternionf::FromMatrix3x3(orient);
			aabb.center = center;
			aabb.extents = extents;
		}

		void Update(const vector3f &position, const matrix3x3f &orient)
		{
			rotation = Quaternionf::FromMatrix3x3(orient);
			aabb.center = position;
		}

		bool IntersectsRay(const vector3f &o, const vector3f &d, float &out_t) const
		{
			// make a ray origin relative to the AABB center in axis-aligned rotation space
			return aabb.IntersectsRay(rotation * (o - aabb.center), rotation * d, out_t);
		}
	};

	struct SphereCollider {
		vector3f center;
		float radius;
		uint32_t action;

		SphereCollider(vector3f c, float r) :
			center(c),
			radius(r)
		{}

		bool Contains(const vector3f &p) const
		{
			return (p - center).LengthSqr() <= radius * radius;
		}

		// Optimized version of ray-sphere intersection test
		bool IntersectsRay(const vector3f &o, const vector3f &d, float &out_t) const
		{
			vector3f l = center - o;

			const float tc = l.Dot(d);
			const float r2 = radius * radius;
			const float d2 = l.LengthSqr() - tc * tc;
			if (r2 < d2) return false;
			const float det = sqrt(r2 - d2);
			out_t = tc - det;

			return tc >= 0;
		}
	};

	class InteractionScene {
	public:
		InteractionScene();
		~InteractionScene();

		static constexpr uint32_t INVALID_ID = -1U;
		static constexpr uint32_t BOX_BIT = 1U << 30;
		static constexpr size_t NO_ACTION = -1UL;

		uint32_t AddBoxTrigger(size_t action, const vector3f &position, const matrix3x3f &orient, const vector3f &extents);
		uint32_t AddSphereTrigger(size_t action, const vector3f &position, float radius);

		void UpdateTriggerPos(uint32_t id, const vector3f &position, const matrix3x3f &orient);

		size_t GetTriggerAction(uint32_t id) const;

		size_t TraceRay(const vector3f &o, const vector3f &d);

		void DrawDebug(Graphics::Renderer *r, Graphics::Material *mat, const matrix4x4f &viewTransform);

	private:
		std::vector<BoxCollider> m_boxTriggers;
		std::vector<SphereCollider> m_sphereTriggers;

		size_t m_lastAction;
	};

} // namespace Cockpit
