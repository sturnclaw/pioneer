// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "editor/EditorDraw.h"
#include "editor/UndoSystem.h"

namespace Editor::Draw {

	template<typename T>
	struct SpanHelper {
		template<size_t N>
		SpanHelper(T (&arr)[N]) :
			data(arr),
			size(N)
		{}

		T *data;
		size_t size;
	};

	template<typename T>
	void EditOptions(std::string_view label, const char *name, SpanHelper<const char * const>options, UndoSystem *undo, T *val)
	{
		size_t selected = size_t(*val);
		const char *preview = selected < options.size ? options.data[selected] : "<invalid>";

		if (ComboUndoHelper(label, name, preview, undo)) {
			if (ImGui::IsWindowAppearing())
				AddUndoSingleValue(undo, val);

			for (size_t idx = 0; idx < options.size; ++idx) {
				if (ImGui::Selectable(options.data[idx], selected == idx))
					*val = T(idx);
			}

			ImGui::EndCombo();
		}
	}

}
