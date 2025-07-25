// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SHIELDS_H_
#define _SHIELDS_H_

/*
 * Mesh shields for ships and other objects.
 */
#include "Color.h"
#include "JsonFwd.h"
#include "RefCounted.h"
#include "matrix4x4.h"
#include "vector3.h"

#include <deque>

namespace Graphics {
	class Renderer;
	class Material;
	struct VertexFormatDesc;
}
namespace SceneGraph {
	class Model;
	class StaticGeometry;
} // namespace SceneGraph

class Shields {
public:
	struct Shield {
		Shield(const Color3ub &color, const matrix4x4f &matrix, SceneGraph::StaticGeometry *sg);
		Color3ub m_colour; // I'm English, so it's "colour" ;)
		matrix4x4f m_matrix;
		RefCountedPtr<SceneGraph::StaticGeometry> m_mesh;
	};

	Shields();
	virtual ~Shields();
	virtual void SaveToJson(Json &jsonObj);
	virtual void LoadFromJson(const Json &jsonObj);

	void ApplyModel(SceneGraph::Model *model);
	void ClearModel();

	void SetEnabled(const bool on) { m_enabled = on; }
	void Update(const float coolDown, const float shieldStrength);
	void SetColor(const Color3ub &);
	void AddHit(const vector3d &hitPos);

	static void Init(Graphics::Renderer *);
	static void Uninit();

	SceneGraph::StaticGeometry *GetFirstShieldMesh();

protected:
	struct Hits {
		Hits(const vector3d &_pos, const Uint32 _start, const Uint32 _end);
		vector3d pos;
		Uint32 start;
		Uint32 end;
	};

	RefCountedPtr<Graphics::Material> FindOrCreateMaterial(Graphics::Renderer *r, const Graphics::VertexFormatDesc &vtxFormat);

	std::deque<Hits> m_hits;
	std::vector<Shield> m_shields;
	// RefCountedPtr<Graphics::Material> m_shieldMaterial;
	std::vector<std::pair<uint64_t, RefCountedPtr<Graphics::Material>>> m_shieldMaterials;

	bool m_enabled;

	static bool s_initialised;
};

#endif
