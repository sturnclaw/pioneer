// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GEOM_H
#define _GEOM_H

#include "../matrix4x4.h"
#include "../vector3.h"

#include <vector>

struct CollisionContact;
class GeomTree;
struct isect_t;
struct Sphere;

class Geom {
public:
	Geom(const GeomTree *geomtree, const matrix4x4d &m, const vector3d &pos, void *data);
	void MoveTo(const matrix4x4d &m);
	void MoveTo(const matrix4x4d &m, const vector3d &pos);
	inline const matrix4x4d &GetInvTransform() const { return m_invOrient; }
	inline const matrix4x4d &GetTransform() const { return m_orient; }
	inline const vector3d &GetPosition() const { return m_pos; }
	inline void Enable() { m_active = true; }
	inline void Disable() { m_active = false; }
	inline bool IsEnabled() const { return m_active; }
	inline const GeomTree *GetGeomTree() const { return m_geomtree; }

	std::vector<CollisionContact> Collide(Geom *b) const;
	void CollideSphere(Sphere &sphere, void (*callback)(CollisionContact *)) const;
	inline void *GetUserData() const { return m_data; }
	inline void SetMailboxIndex(int idx) { m_mailboxIndex = idx; }
	inline int GetMailboxIndex() const { return m_mailboxIndex; }
	inline void SetGroup(int g) { m_group = g; }
	inline int GetGroup() const { return m_group; }

private:
	// Note: these functions could easily be made static and extracted into a separate collision system
	void CollideEdgesWithTrisOf(std::vector<CollisionContact> &contacts, size_t maxContacts, const Geom *b, const matrix4x4d &transTo) const;
	void CollideEdgeTris(std::vector<CollisionContact> &contacts, const matrix4x4d &transToB, const Geom *b, uint32_t edgeIdx, uint32_t triIdx, std::vector<uint32_t> &isect_buf) const;

	// double-buffer position so we can keep previous position
	vector3d m_pos;
	const GeomTree *m_geomtree;
	matrix4x4d m_orient, m_invOrient;

public:
	matrix4x4d m_animTransform;

private:
	void *m_data;
	int m_group;
	int m_mailboxIndex; // used to avoid duplicate collisions
	bool m_active;
};

#endif /* _GEOM_H */
