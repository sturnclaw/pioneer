// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

/**
 * Saving and loading a model from a binary format,
 * completely without Assimp
 * Nodes are expected to implement a Save method to
 * serialize their internals
 */

#include "BaseLoader.h"
#include "../SceneGraph.h"
#include "FileSystem.h"
#include <functional>
#include <type_traits>

namespace Serializer {
	class Reader;
	class Writer;
} // namespace Serializer

namespace SceneGraph {
	class Label3D;
	class Model;

	class AdapterSGM : public BaseLoader {
	public:
		AdapterSGM(Graphics::Renderer *);
		
		// Save a model to disk at the specified path.
		void Save(const std::string &filepath, Model *m);

		// Load a model from an SGM file at the specified path.
		Model *Load(const std::string &filepath);

		// Load a model from the specified binary data blob, using basename as the name of the model.
		Model *Load(const std::string &basename, RefCountedPtr<FileSystem::FileData> binfile);

		//if you implement any new node types, you must also register a loader function
		//before calling Load.
		void RegisterLoader(const std::string &typeName, std::function<Node *(NodeDatabase &)>);

	private:
		Graphics::Renderer *m_renderer;
		Model *m_model;

		Model *CreateModel(const std::string &filename, Serializer::Reader &);
		void SaveMaterials(Serializer::Writer &, Model *m);
		void LoadMaterials(Serializer::Reader &);
		void SaveAnimations(Serializer::Writer &, Model *m);
		void LoadAnimations(Serializer::Reader &);
		ModelDefinition FindModelDefinition(const std::string &);

		Node *LoadNode(Serializer::Reader &);
		void LoadChildren(Serializer::Reader &, Group *parent);
		//this is a very simple loader so it's implemented here
		static Label3D *LoadLabel3D(NodeDatabase &);

		bool m_patternsUsed;
		std::map<std::string, std::function<Node *(NodeDatabase &)>> m_loaders;
	};
} // namespace SceneGraph
