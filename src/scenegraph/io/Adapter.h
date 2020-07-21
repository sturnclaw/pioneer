// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "scenegraph/Model.h"
#include "LoaderDefinitions.h"

namespace SceneGraph {
    class ModelLoader;

    class IOAdapter {
    public:
		IOAdapter(Graphics::Renderer *renderer) :
			m_renderer(renderer)
		{ }

        // Given the path to a .model file, load the model from disk.
        virtual Model *LoadFile(const std::string &modelPath) = 0;

		// Take the given Model and save it to disk.
        virtual bool SaveModel(const Model *model, std::string path) { return false; }

        // Returns true if the format is appropriate for saving models with the
        // specified extension (includes collision, mesh data, etc.)
        virtual bool CanSaveFile(std::string path) const { return false; }

        // Return true if this is an extension that we know how to parse.
        virtual bool CanLoadFile(std::string path) const = 0;

    protected:
        friend class ModelLoader;
        Graphics::Renderer *m_renderer;

		Model *m_model;
		std::string m_curPath; //path of current model file
	};
}
