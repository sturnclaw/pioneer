-- Copyright © 2008-2020 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local widgets = require '.core'
local pigui = require 'Engine'.pigui


-- Class: Widgets.Text
-- Inherits from <Base>
widgets.Text = widgets.Base {
	[0] = "",

	-- Parameter: font
	--
	-- The font the text will be rendered with
	font = { name = "pionillium", size = 18 },

	-- Parameter: color
	--
	-- The color the text will be rendered with
	color = Color(255, 255, 255),

	-- Parameter: wrap
	--
	-- If enabled, the text will wrap based on the widget's calculated
	-- horizontal size; setting size.x = 0 will cause problems unless
	-- the widget has horizontal fill enabled.
	wrap = false,

	__layout = function(self, context)
		if self.wrap then
			local wrap_width = self.__extent.x or self.size.x
			self.size.y = pigui.CalcTextSize(self[1], nil, wrap_width)
		else
			self.size = pigui.CalcTextSize(self[1])
		end
		widgets.Base.__layout(self, context)
	end,

	__draw = function(self)
		pigui:PushFont(self.font.name, self.font.size)
		pigui.SetCursorPos(self.root:__transform(self.__pos))
		if self.wrap then
			pigui.PushTextWrapPos(self.root:__transform(self.__pos + self.__extent).x)
			pigui.TextColored(self.color, self[1])
			pigui.PopTextWrapPos()
		else
			pigui.TextColored(self.color, self[1]);
		end
		pigui:PopFont();
	end
}

-- Class: Widgets.TextWrapped
-- Inherits from <Text>
-- FIXME: needs a bit more thought regarding wrapping; the text can easily overflow
widgets.TextWrapped = widgets.Text {
	anchor = "HFill",
	wrap = true
}

return widgets
