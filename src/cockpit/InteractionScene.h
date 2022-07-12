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
	vector3f min, max;

	vector3f GetExtents() const
	{
		return (max - min) * 0.5f;
	}

	vector3f GetCenter() const
	{
		return (max + min) * 0.5f;
	}

	void Update(const vector3f &p)
	{
		max.x = std::max(max.x, p.x);
		max.y = std::max(max.y, p.y);
		max.z = std::max(max.z, p.z);

		min.x = std::min(min.x, p.x);
		min.y = std::min(min.y, p.y);
		min.z = std::min(min.z, p.z);
	}

	bool Contains(const vector3f &p) const
	{
		return ((p.x >= min.x) && (p.x <= max.x) &&
			(p.y >= min.y) && (p.y <= max.y) &&
			(p.z >= min.z) && (p.z <= max.z));
	}

	bool Intersects(const AABBf &o) const
	{
		return (min.x < o.max.x) && (max.x > o.min.x) &&
			(min.y < o.max.y) && (max.y > o.min.y) &&
			(min.z < o.max.z) && (max.z > o.min.z);
	}

	// Adapted from https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
	bool IntersectsRay(const vector3f &o, const vector3f &d, float &out_t) const
	{
		vector3f invdir = 1.0f / d;

		vector3f t1 = (min - o) * invdir;
		vector3f t2 = (max - o) * invdir;

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
	// An OBB can be constructed by taking the center and inverse rotation of
	// a transformed AABB, applying the inverse rotation, and applying extents.
	struct BoxCollider {
		AABBf aabb;
		Quaternionf inv_rot;
		size_t action;

		BoxCollider(const vector3f &center, const vector3f &extents, const matrix3x3f &orient)
		{
			inv_rot = ~Quaternionf::FromMatrix3x3(orient);
			aabb.min = inv_rot * center - extents;
			aabb.max = inv_rot * center + extents;
		}

		void Update(const vector3f &position, const matrix3x3f &orient)
		{
			const vector3f extents = aabb.GetExtents();
			inv_rot = ~Quaternionf::FromMatrix3x3(orient);
			aabb.min = inv_rot * position - extents;
			aabb.max = inv_rot * position + extents;
		}

		bool Contains(const vector3f &p) const
		{
			return aabb.Contains(inv_rot * p);
		}

		bool IntersectsRay(const vector3f &o, const vector3f &d, float &out_t) const
		{
			return aabb.IntersectsRay(inv_rot * o, inv_rot * d, out_t);
		}
	};

	struct SphereCollider {
		vector3f center;
		float radius;
		size_t action;

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
