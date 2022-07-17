// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "InteractionScene.h"

#include "graphics/Renderer.h"
#include "matrix3x3.h"

#include "graphics/Types.h"
#include "graphics/VertexArray.h"
#include "profiler/Profiler.h"

using namespace Cockpit;

InteractionScene::InteractionScene() :
	m_lastAction(NO_ACTION)
{

}

InteractionScene::~InteractionScene()
{

}

uint32_t InteractionScene::AddBoxTrigger(size_t action, const vector3f &position, const matrix3x3f &orient, const vector3f &extents)
{
	BoxCollider collider(position, extents, orient);
	collider.action = action;
	m_boxTriggers.push_back(collider);

	return (m_boxTriggers.size() - 1) | BOX_BIT;
}

uint32_t InteractionScene::AddSphereTrigger(size_t action, const vector3f &position, float radius)
{
	SphereCollider collider(position, radius);
	collider.action = action;
	m_sphereTriggers.push_back(collider);

	return (m_sphereTriggers.size() - 1);
}

void InteractionScene::UpdateTriggerPos(uint32_t id, const vector3f &position, const matrix3x3f &orient)
{
	if (id == INVALID_ID)
		return;

	if (id & BOX_BIT) {
		BoxCollider &collider = m_boxTriggers[id & ~BOX_BIT];
		collider.Update(position, orient);
	} else {
		SphereCollider &collider = m_sphereTriggers[id];
		collider.center = position;
	}
}

size_t InteractionScene::GetTriggerAction(uint32_t id) const
{
	if (id == INVALID_ID)
		return 0;

	if (id & BOX_BIT) {
		return m_boxTriggers[id & ~BOX_BIT].action;
	} else {
		return m_sphereTriggers[id].action;
	}
}

size_t InteractionScene::TraceRay(const vector3f &o, const vector3f &d)
{
	PROFILE_SCOPED()

	size_t ret_action = NO_ACTION;
	float ret_distance = INFINITY;

	// Brute-force loop over all colliders in scene, could be made faster
	// with an acceleration structure...

	for (const SphereCollider &coll : m_sphereTriggers) {
		float dist = -1.0;
		if (!coll.IntersectsRay(o, d, dist))
			continue;

		if (dist < ret_distance) {
			ret_distance = dist;
			ret_action = coll.action;
		}
	}

	for (const BoxCollider &coll : m_boxTriggers) {
		float dist = -1.0;
		if (!coll.IntersectsRay(o, d, dist))
			continue;

		if (dist < ret_distance) {
			ret_distance = dist;
			ret_action = coll.action;
		}
	}

	m_lastAction = ret_action;
	return ret_action;
}

void InteractionScene::DrawDebug(Graphics::Renderer *r, Graphics::Material *m, const matrix4x4f &viewTransform)
{
	const Color sphereColor = Color(68, 255, 0);
	const Color boxColor = Color(255, 180, 0);
	const Color activeColor = Color(120, 180, 255);

	Graphics::VertexArray lines(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE);

	// Draw each sphere-collider as three circles
	for (const SphereCollider &coll : m_sphereTriggers) {
		const Color color = coll.action == m_lastAction ? activeColor : sphereColor;

		const float step = M_PI / 16;
		for (float i = 0; i < M_PI * 2; i += step) {
			float sa = sinf(i) * coll.radius;
			float ca = cosf(i) * coll.radius;
			float sb = sinf(i + step) * coll.radius;
			float cb = cosf(i + step) * coll.radius;

			lines.Add(coll.center + vector3f(sa, ca, 0.0), color);
			lines.Add(coll.center + vector3f(sb, cb, 0.0), color);

			lines.Add(coll.center + vector3f(sa, 0.0, ca), color);
			lines.Add(coll.center + vector3f(sb, 0.0, cb), color);

			lines.Add(coll.center + vector3f(0.0, sa, ca), color);
			lines.Add(coll.center + vector3f(0.0, sb, cb), color);
		}
	}

	// Draw each box-collider as an AABB in world space
	for (const BoxCollider &coll : m_boxTriggers) {
		const Color color = coll.action == m_lastAction ? activeColor : boxColor;
		// Make a matrix to rotate the given AABB back into world space
		matrix3x3f rot = coll.inv_rot.ToMatrix3x3<float>();
		const AABBf &aabb = coll.aabb;

		const vector3f verts[16] = {
			rot * vector3f(aabb.min.x, aabb.min.y, aabb.min.z),
			rot * vector3f(aabb.max.x, aabb.min.y, aabb.min.z),
			rot * vector3f(aabb.max.x, aabb.max.y, aabb.min.z),
			rot * vector3f(aabb.min.x, aabb.max.y, aabb.min.z),
			rot * vector3f(aabb.min.x, aabb.min.y, aabb.min.z),
			rot * vector3f(aabb.min.x, aabb.min.y, aabb.max.z),
			rot * vector3f(aabb.max.x, aabb.min.y, aabb.max.z),
			rot * vector3f(aabb.max.x, aabb.min.y, aabb.min.z),

			rot * vector3f(aabb.max.x, aabb.max.y, aabb.max.z),
			rot * vector3f(aabb.min.x, aabb.max.y, aabb.max.z),
			rot * vector3f(aabb.min.x, aabb.min.y, aabb.max.z),
			rot * vector3f(aabb.max.x, aabb.min.y, aabb.max.z),
			rot * vector3f(aabb.max.x, aabb.max.y, aabb.max.z),
			rot * vector3f(aabb.max.x, aabb.max.y, aabb.min.z),
			rot * vector3f(aabb.min.x, aabb.max.y, aabb.min.z),
			rot * vector3f(aabb.min.x, aabb.max.y, aabb.max.z),
		};

		for (unsigned int i = 0; i < 7; i++) {
			lines.Add(verts[i], color);
			lines.Add(verts[i + 1], color);
		}

		for (unsigned int i = 8; i < 15; i++) {
			lines.Add(verts[i], color);
			lines.Add(verts[i + 1], color);
		}
	}

	r->SetTransform(viewTransform);
	r->DrawBuffer(&lines, m);
}
