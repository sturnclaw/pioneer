
#pragma once

#include "graphics/Renderer.h"
#include "Adapter.h"
#include "scenegraph/io/LoaderDefinitions.h"

#include <vector>
#include <string>

namespace SceneGraph {
    class ModelLoader {
    public:
        /*
         * Create a new ModelLoader.
         */
        ModelLoader(
            Graphics::Renderer *renderer,
            bool doLog = true,
            bool preferSourceFiles = false
        );

        const std::vector<std::string> &GetLogMessages();

        // Load a model from the models/ data directory. Expects a filename without extension.
        Model *LoadModel(const std::string filename)
        {
            return Deprecated_LoadModel(filename, "models");
        }

        // Load a model from the specified directory in the tree.
        // modelName should be supplied without extension.
        Model *LoadModel(const std::string dir, std::string modelName);
        
        // Save a model to the specified path, including extension.
        bool SaveModel(Model *model, std::string path);
    
    private:
        ModelDefinition LoadModelDefinition(Json &model_def);

        Model *Deprecated_LoadModel(const std::string filename, const std::string basepath);

        Model *m_model;
        Graphics::Renderer *m_renderer;
        bool m_loadCompiled;
        bool m_doLog;
        std::vector<std::string> m_logMessages;
        std::vector<std::unique_ptr<IOAdapter>> m_adapters;
	};
}
