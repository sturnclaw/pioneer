// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "scenegraph/Model.h"
#include "LoaderDefinitions.h"

namespace SceneGraph {
    class IOAdapter {
    public:
		IOAdapter(Graphics::Renderer *renderer, bool doLog, std::vector<std::string> &logFile) :
			m_renderer(renderer), m_doLog(doLog), m_logFile(logFile)
		{ }

        // Given a ModelDefinition, load the model from disk.
		virtual Model *LoadModel(ModelDefinition &def) = 0;

        // Take the given Model and save it to disk in the appropriate format.
        virtual bool SaveModel(Model *model, std::string path) = 0;

        // Returns true if the format is appropriate for saving models (includes collision, mesh data, etc.)
        // Otherwise, the adapter can only be used for loading models.
        virtual bool CanSaveModels() = 0;

    protected:
        Graphics::Renderer *m_renderer;
        std::vector<std::string> &m_logFile;
        bool m_doLog;
	};
}
