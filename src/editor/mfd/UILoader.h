// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "JsonFwd.h"

#include <string>

class ImFont;

namespace Editor {

	struct UIObject;
	struct UIStyle;

	class UILoader {
	public:

		// Helper class to provide information to the loader
		class Delegate {
		public:

			virtual UIObject *CreateNewObject() = 0;
			virtual UIStyle *CreateNewStyle() = 0;

			virtual ImFont *GetFont(std::string_view name, size_t size) = 0;
			virtual std::string_view GetFontName(ImFont *font) = 0;

			virtual std::string_view GetStyleName(UIStyle *style) = 0;
			virtual UIStyle *GetStyle(std::string_view name) = 0;

		};

		UILoader(Delegate *delegate);
		~UILoader();

		Json SaveObject(const UIObject *object);
		UIObject *LoadObject(const Json &obj);

		Json SaveStyle(const UIStyle *style);
		UIStyle *LoadStyle(const Json &obj);

	private:
		Delegate *m_delegate;
	};

} // namespace Editor
