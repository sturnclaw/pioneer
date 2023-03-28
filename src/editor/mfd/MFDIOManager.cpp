// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "MFDIOManager.h"

#include "MFDEditor.h"

#include "UIObject.h"
#include "UIView.h"

#include "FileSystem.h"
#include "Json.h"
#include "core/Log.h"

using namespace Editor;


MFDIOManager::MFDIOManager(MFDEditor *editor) :
	m_editor(editor)
{
	m_loader.reset(new UILoader(this));
}

MFDIOManager::~MFDIOManager()
{
}

UIObject *MFDIOManager::CreateNewObject()
{
	return m_editor->CreateNewObject();
}

UIStyle *MFDIOManager::CreateNewStyle()
{
	return m_editor->CreateNewStyle();
}

UIStyle *MFDIOManager::GetStyle(std::string_view name)
{
	for (auto &pair : m_editor->GetRootView()->GetStyles())
		if (pair.first == name)
			return pair.second.get();

	return nullptr;
}

std::string_view MFDIOManager::GetStyleName(UIStyle *style)
{
	return m_editor->GetRootView()->GetStyleName(style);
}

ImFont *MFDIOManager::GetFont(std::string_view name, size_t size)
{
	return m_editor->GetRootView()->GetOrLoadFont(name, size);
}

std::string_view MFDIOManager::GetFontName(ImFont *font)
{
	return m_editor->GetRootView()->GetFontName(font);
}

// ============================================================================

bool MFDIOManager::SaveLayout(std::string_view filepath, std::string_view stylePath)
{
	std::string path = FileSystem::NormalisePath(std::string(filepath));
	path = FileSystem::JoinPath(FileSystem::GetDataDir(), path);

	FILE *saveFile = fopen(path.c_str(), "w");
	if (!saveFile) {
		Log::Error("Couldn't open path '{}' to save MFD layout!", path);
		return false;
	}

	Json root = m_loader->SaveObject(m_editor->GetRootView()->GetRoot());

	Json fileObj = Json::object();
	fileObj["stylePath"] = stylePath;
	fileObj["objects"] = std::move(root);

	Json styleRoot = Json::object();

	for (auto &pair : m_editor->GetRootView()->GetStyles()) {
		styleRoot[pair.first] = m_loader->SaveStyle(pair.second.get());
	}

	fileObj["inlineStyles"] = std::move(styleRoot);

	std::string layoutData = fileObj.dump(1, '\t');

	fwrite(layoutData.c_str(), 1, layoutData.size(), saveFile);
	fclose(saveFile);

	return true;
}

bool MFDIOManager::SaveStyles(std::string_view filepath)
{
	std::string path = FileSystem::NormalisePath(std::string(filepath));
	path = FileSystem::JoinPath(FileSystem::GetDataDir(), path);

	FILE *saveFile = fopen(path.c_str(), "w");
	if (!saveFile) {
		Log::Error("Couldn't open path '{}' to save styles!", path);
		return false;
	}

	Json root = Json::object();

	for (auto &pair : m_editor->GetRootView()->GetStyles()) {
		root[pair.first] = m_loader->SaveStyle(pair.second.get());
	}

	std::string styleData = root.dump(1, '\t');

	fwrite(styleData.c_str(), 1, styleData.size(), saveFile);
	fclose(saveFile);

	return true;
}

// ============================================================================

UIObject *MFDIOManager::LoadLayout(std::string_view filepath)
{
	std::string path = FileSystem::NormalisePath(std::string(filepath));
	path = FileSystem::GetRelativePath(FileSystem::GetDataDir(), path);

	RefCountedPtr<FileSystem::FileData> data = FileSystem::gameDataFiles.ReadFile(path);

	if (!data.Valid()) {
		Log::Error("Couldn't open data path '{}' to load MFD layout!", path);
		return nullptr;
	}

	Json root = Json::parse(data->GetData(), data->GetData() + data->GetSize());
	if (!root.is_object() || root.empty()) {
		Log::Error("MFD Layout file '{}' is invalid!", path);
		return nullptr;
	}

	std::string_view stylePath = root["stylePath"];
	if (!stylePath.empty()) {
		m_editor->SetEditedStyles(stylePath);
		LoadStyles(stylePath);
	}

	const Json &styleRoot = root["inlineStyles"];
	if (styleRoot.is_object()) {
		for (auto &pair : styleRoot.items()) {
			UIStyle *style = m_loader->LoadStyle(pair.value());
			m_editor->GetRootView()->GetStyles().emplace(pair.key(), style);
		}
	}

	return m_loader->LoadObject(root["objects"]);
}

bool MFDIOManager::LoadStyles(std::string_view filepath)
{
	std::string path = FileSystem::NormalisePath(std::string(filepath));
	path = FileSystem::GetRelativePath(FileSystem::GetDataDir(), path);

	RefCountedPtr<FileSystem::FileData> data = FileSystem::gameDataFiles.ReadFile(path);

	if (!data.Valid()) {
		Log::Error("Couldn't open data path '{}' to load styles!", path);
		return false;
	}

	Json root = Json::parse(data->GetData(), data->GetData() + data->GetSize());
	if (!root.is_object() || root.empty()) {
		Log::Error("Style file '{}' is invalid!", path);
		return false;
	}

	for (auto &pair : root.items()) {
		UIStyle *style = m_loader->LoadStyle(pair.value());
		m_editor->GetRootView()->GetStyles().emplace(pair.key(), style);
	}

	return true;
}
