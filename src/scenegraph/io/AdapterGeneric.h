// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

/**
 * Model loader using Assimp
 * This loader handles loading OBJ and Collada meshes from disk.
 */
#include "Adapter.h"
#include "../CollisionGeometry.h"
#include "../StaticGeometry.h"
#include "graphics/Material.h"

struct aiMatrix4x4;
struct aiNode;
struct aiMesh;
struct aiScene;
struct aiNodeAnim;

namespace SceneGraph {

	class AdapterGeneric : public IOAdapter {
	public:
		AdapterGeneric(Graphics::Renderer *r, bool logWarnings = false);

		Model *LoadFile(std::string filename) override;

		// The adapter could load more, but for now we only handle OBJ and Collada.
		bool CanLoadFile(std::string path) const override {
			size_t pos = path.find_last_of('.');
			std::string extension = path.substr(pos, path.size() - pos);
			return extension == ".dae" || extension == ".obj";
		}

	protected:
		bool m_doLog;
		bool m_mostDetailedLod;
		std::vector<std::string> m_logMessages;
		std::string m_curMeshDef; //for logging

		RefCountedPtr<Group> m_thrustersRoot;
		RefCountedPtr<Group> m_billboardsRoot;

		bool CheckKeysInRange(const aiNodeAnim *, double start, double end);
		matrix4x4f ConvertMatrix(const aiMatrix4x4 &) const;
		Model *CreateModel(ModelDefinition &def);
		//load one mesh file so it can be added to the model scenegraph. Materials should be created before this!
		RefCountedPtr<Node> LoadMesh(const std::string &filename, const AnimList &animDefs);
		void AddLog(const std::string &);
		//detect animation overlap
		void CheckAnimationConflicts(const Animation *, const std::vector<Animation *> &);
		void ConvertAiMeshes(std::vector<RefCountedPtr<StaticGeometry>> &, const aiScene *); //model is only for material lookup
		void ConvertAnimations(const aiScene *, const AnimList &, Node *meshRoot);
		void ConvertNodes(aiNode *node, Group *parent, std::vector<RefCountedPtr<StaticGeometry>> &meshes, const matrix4x4f &);
		void CreateLabel(Group *parent, const matrix4x4f &);
		void CreateThruster(const std::string &name, const matrix4x4f &nodeTrans);
		void CreateNavlight(const std::string &name, const matrix4x4f &nodeTrans);
		RefCountedPtr<CollisionGeometry> CreateCollisionGeometry(RefCountedPtr<StaticGeometry>, unsigned int collFlag);
		void LoadCollision(const std::string &filename);

		unsigned int GetGeomFlagForNodeName(const std::string &);
	};

} // namespace SceneGraph
