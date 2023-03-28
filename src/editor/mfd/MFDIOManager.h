// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "UILoader.h"

namespace Editor {

	class MFDEditor;

	class MFDIOManager : public UILoader::Delegate {
	public:
		MFDIOManager(MFDEditor *editor);
		~MFDIOManager();

		// Implementation of UILoader::Delegate
		virtual UIObject *CreateNewObject() override;
		virtual UIStyle *CreateNewStyle() override;

		virtual ImFont *GetFont(std::string_view name, size_t size) override;
		virtual std::string_view GetFontName(ImFont *font) override;

		virtual UIStyle *GetStyle(std::string_view name) override;
		virtual std::string_view GetStyleName(UIStyle *style) override;

		// Save the currently-authored layout to a file
		bool SaveLayout(std::string_view filepath, std::string_view stylePath);

		// Save the current set of edited styles
		bool SaveStyles(std::string_view filepath);

		// Load a UI layout from the given file
		UIObject *LoadLayout(std::string_view filepath);

		// Load a set of styles from the given file
		bool LoadStyles(std::string_view filepath);

	private:
		MFDEditor *m_editor;

		std::unique_ptr<UILoader> m_loader;

	};

} // namespace Editor
