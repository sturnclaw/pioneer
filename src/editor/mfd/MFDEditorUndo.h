// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "UIObject.h"
#include "UIView.h"

#include "editor/UndoSystem.h"

namespace Editor {

	// UndoStep helper to handle adding or deleting a child UIObject from a parent
	class UndoAddRemoveChild : public UndoStep {
	public:
		UndoAddRemoveChild(UIObject *parent, UIObject *add, size_t idx) :
			m_parent(parent),
			m_add(add),
			m_idx(idx)
		{
			Swap();
		}

		UndoAddRemoveChild(UIObject *parent, UIObject *add) :
			m_parent(parent),
			m_add(add),
			m_idx(parent->children.size())
		{
			Swap();
		}

		UndoAddRemoveChild(UIObject *parent, size_t idx) :
			m_parent(parent),
			m_add(nullptr),
			m_idx(idx)
		{
			Swap();
		}

		void Undo() override { Swap(); }
		void Redo() override { Swap(); }

	private:
		void Swap() {
			if (m_add) {
				m_parent->AddChild(m_add.release(), m_idx);
			} else {
				m_add.reset(m_parent->RemoveChild(m_idx));
			}
		}

		UIObject *m_parent;
		std::unique_ptr<UIObject> m_add;
		size_t m_idx;
	};

	// UndoStep to handle reordering a given UIObject in its parent
	class UndoReorderChild : public UndoStep {
	public:
		UndoReorderChild(UIObject *parent, size_t oldIdx, size_t newIdx) :
			m_parent(parent),
			m_old(oldIdx),
			m_new(newIdx)
		{
			Redo();
		}

		void Undo() override { m_parent->ReorderChild(m_new, m_old); }
		void Redo() override { m_parent->ReorderChild(m_old, m_new); }

	private:
		UIObject *m_parent;
		size_t m_old;
		size_t m_new;
	};

	class UndoAddRemoveStyle : public UndoStep {
	public:
		// Add a new style to the view
		UndoAddRemoveStyle(UIView *view, std::string_view key, UIStyle *newStyle) :
			m_view(view),
			m_oldKey(),
			m_newKey(key),
			m_style(newStyle)
		{
			Swap();
		}

		// Delete a style from the view
		UndoAddRemoveStyle(UIView *view, std::string_view key) :
			m_view(view),
			m_oldKey(key),
			m_newKey(),
			m_style(nullptr)
		{
			Swap();
		}

		// Move (rename) a style from an old key to a new key
		UndoAddRemoveStyle(UIView *view, std::string_view oldKey, std::string_view newKey) :
			m_view(view),
			m_oldKey(oldKey),
			m_newKey(newKey),
			m_style(nullptr)
		{
			Swap();
		}

		void Undo() override { Swap(); }
		void Redo() override { Swap(); }

	private:
		void Swap()
		{
			if (!m_oldKey.empty()) {
				m_style = std::move(m_view->GetStyles()[m_oldKey]);
				m_view->GetStyles().erase(m_oldKey);
			}

			if (!m_newKey.empty()) {
				m_view->GetStyles().emplace(m_newKey, std::move(m_style));
			}

			std::swap(m_oldKey, m_newKey);
		}

		UIView *m_view;
		std::string m_oldKey;
		std::string m_newKey;
		std::unique_ptr<UIStyle> m_style;
	};

} // namespace Editor
