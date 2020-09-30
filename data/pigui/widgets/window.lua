-- Copyright © 2008-2020 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local widgets = require '.core'
local pigui = require 'Engine'.pigui

-- Class: Widgets.Window
-- Inherits from <Base>
widgets.Window = widgets.Layout {
	anchor = "Center",
	layout = "Column",
	justify = "Start",

	-- Property: flags
	--
	-- Window flags passed to ImGui controlling the visual appearance of the window.
	flags = pigui.WindowFlags { },

	-- Property: title
	--
	-- The window's title; displayed by ImGui if the title bar is enabled and used
	-- as part of the ID stack. If this is not given, <Layout.id> must be present.
	title = "",

	-- Property: padding
	--
	-- Defines the padding between the window border and the window's contents
	padding = Vector2(8, 8),

	-- Property: scrollable
	--
	-- If set, the window will only resize to content along the cross-axis, and should
	-- have its size defined programmatically
	scrollable = false,

	__layout = function(self, context)
		widgets.Base.__layout(self, context)
		-- default container behavior is Free|Center

		if not (#self.title > 0 or self.id) then
			widgets.logOnce(self, "No title or id attribute specified for Window, inserting (unstable) random number!")
			self.id = widgets.makeId()
		end

		self.__container = widgets.Layout {
			root = self,
			layout = self.layout,
			justify = self.justify,
			wrap = self.wrap,
			margin = { self.padding.x, self.padding.y, self.padding.x, self.padding.y },

			table.unpack(self)
		}

		self.__container:__layout(context)
		context:insert(self.__item, self.__container.__item)
	end,

	__update = function(self, context)
		widgets.Base.__update(self, context)
		self.__container:update(context)
	end,

	-- convert root-relative coordinates into ImGui coordinates
	__transform = function(self, pos)
		return pos - pigui.GetWindowPos()
	end,

	__draw = function(self)
		pigui.PushStyleVar("WindowPadding", self.padding)
		-- TODO: resizable windows
		pigui.SetNextWindowPos(self.__pos)
		pigui.SetNextWindowSize(self.__extent)
		local open = pigui.Begin(#self.title > 0 and self.title or ("##" .. self.id))
		pigui.PopStyleVar(1)
		if open then
			local ok, err = ui.pcall(self.__container.draw, self.__container)
			if not ok then widgets.logOnce(self, err) end
		end
		pigui.End()
	end
}

widgets.Popup = widgets.Layout {

}

return widgets
