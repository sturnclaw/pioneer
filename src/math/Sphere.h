// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#ifndef _SPHERE_H
#define _SPHERE_H

#include "vector3.h"

struct SSphere {
	SSphere() :
		m_centre(vector3d(0.0)),
		m_radius(1.0) {}
	SSphere(const double rad) :
		m_centre(vector3d(0.0)),
		m_radius(rad) {}
	SSphere(const vector3d &centre, const double rad) :
		m_centre(centre),
		m_radius(rad) {}

	// Adapted from Ysaneya here: http://www.gamedev.net/blog/73/entry-1666972-horizon-culling/
	bool HorizonCulling(const vector3d &view, const SSphere &obj) const;

	vector3d m_centre;
	double m_radius;
};

#endif /* _SPHERE_H */
