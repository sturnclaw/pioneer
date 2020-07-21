
#include "FileSystem.h"
#include "profiler/Profiler.h"
#include "utils.h"
#include "ModelLoader.h"
#include "AdapterGeneric.h"
#include "AdapterSGM.h"
#include "graphics/Renderer.h"

using namespace SceneGraph;

ModelLoader::ModelLoader(Graphics::Renderer *renderer, bool doLog, bool preferSourceFiles) :
	m_renderer(renderer),
	m_loadCompiled(!preferSourceFiles),
	m_doLog(doLog)
{
    m_adapters.emplace_back(new AdapterSGM(renderer));
    m_adapters.emplace_back(new AdapterGeneric(renderer));
}

Model *ModelLoader::LoadModel(const std::string folder, const std::string modelname)
{
	PROFILE_SCOPED()
	m_logMessages.clear();

	std::string path = FileSystem::JoinPathBelow(folder, modelname + ".sgm");
	FileSystem::FileInfo info;

	if (m_loadCompiled){
		info = FileSystem::gameDataFiles.Lookup(path);
		if (info.Exists()) {
			AdapterSGM loader(m_renderer);
			Model *model = loader.LoadModel(path);
			if (model)
				return model;
		}
	}

	path = FileSystem::JoinPathBelow(folder, modelname + ".model")
	info = FileSystem::gameDataFiles.Lookup(path);
	if (!info.Exists()) {
		return nullptr;
	}

	const ModelDefinition modelDef = LoadModelDefinition(path);

}

Model *ModelLoader::LoadSGM(std::string path)
{

}

static std::string path_basename(const std::string &str)
{
	size_t pos = str.find_last_of('/');
	if (pos == std::string::npos)
		return str;
	
	return str.substr(pos + 1);
}

static std::string path_stem(const std::string &str)
{
	size_t pos = str.find_last_of('.');
	if (pos == 1 || pos == std::string::npos)
		return str;

	return str.substr(0, pos);
}

Model *ModelLoader::Deprecated_LoadModel(const std::string shortname, const std::string basepath)
{
	PROFILE_SCOPED()
	m_logMessages.clear();

	std::vector<std::string> list_model;
	std::vector<std::string> list_sgm;

	FileSystem::FileSource &fileSource = FileSystem::gameDataFiles;
	for (FileSystem::FileEnumerator files(fileSource, basepath, FileSystem::FileEnumerator::Recurse); !files.Finished(); files.Next()) {
		const FileSystem::FileInfo &info = files.Current();
		const std::string &fpath = info.GetPath();

		//check it's the expected type
		if (info.IsFile()) {
			const std::string name = path_basename(fpath);

			if (path_stem(name) == shortname) {
				if (ends_with_ci(name, ".model"))
					list_model.push_back(fpath);
				
				else if (ends_with_ci(name, ".sgm"))
					list_sgm.push_back(fpath);
			}
		}
	}

	if (m_loadCompiled) {
		for (auto &sgmfile : list_sgm) {
			SceneGraph::AdapterSGM sgmLoader(m_renderer);
			m_model = sgmLoader.LoadModel(sgmfile);
			if (m_model)
				return m_model;
			else
				break; // we'll have to load the non-sgm file
		}
	}

	for (auto &fpath : list_model) {
		RefCountedPtr<FileSystem::FileData> filedata = FileSystem::gameDataFiles.ReadFile(fpath);
		if (!filedata) {
			Output("LoadModel: %s: could not read file\n", fpath.c_str());
			return nullptr;
		}

		//check it's the wanted name & load it
		const FileSystem::FileInfo &info = filedata->GetInfo();
		const std::string name = info.GetName();
		if (name.substr(0, name.length() - 6) == shortname) {
			ModelDefinition modelDefinition;
			try {
				//curPath is used to find textures, patterns,
				//possibly other data files for this model.
				//Strip trailing slash
				m_curPath = info.GetDir();
				assert(!m_curPath.empty());
				if (m_curPath[m_curPath.length() - 1] == '/')
					m_curPath = m_curPath.substr(0, m_curPath.length() - 1);

				Parser p(fileSource, fpath, m_curPath);
				p.Parse(&modelDefinition);
			} catch (ParseError &err) {
				Output("%s\n", err.what());
				throw LoadingError(err.what());
			}
			modelDefinition.name = shortname;
			return CreateModel(modelDefinition);
		}
	}
	throw(LoadingError("File not found"));
}
