-- Copyright © 2008-2020 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local widgets = {}
local pigui = require 'Engine'.pigui
local Rand = require 'Rand'

-- does a shallow-clone of a widget
local function clone(obj, new)
	new = new or {}
	for k, v in pairs(obj or {}) do
		new[k] = new[k] or v
	end
	setmetatable(new, { __index = obj, __call = clone })
	return new
end

-- print-once functionality for logging widget errors etc.
local __log_once = {}
function widgets.logOnce(widget, logmsg)
	if __log_once[widget] then return end
	print("Widget ["..tostring(widget.id or widget).."]:", logmsg)
	__log_once[widget] = true
end

function widgets.resetLogOnce(widget)
	__log_once[widget] = nil
end

-- Random widget id generation
local widget_id_gen = Rand.New("Widgets")
function widgets.makeId()
	return string.format("%x", widget_id_gen:Integer())
end

-- Enum: AnchorMode
-- Controls the effect of margins and sizing on the widget's final size.
--
-- Center	- Center horizontally and vertically, with the left / top margin as offset
-- Fill		- Anchor to all four sides
-- HFill	- Anchor to left+right sides, centered vertically
-- VFill	- Anchor to top+bottom sides, centered horizontally
-- Left		- Anchor to top side, centered on all other sides
-- Top		- Anchor to left side, centered on all other sides
-- Right	- Anchor to right side, centered on all other sides
-- Bottom	- Anchor to right side, centered on all other sides

-- Class: Widgets.Base
widgets.Base = {
	-- Property: size
	--
	-- If the widget's size is non-zero in an axis, it consumes exactly that size,
	-- otherwise it will grow and shrink as needed to fit the requested layout
	size = Vector2(0, 0),

	-- Property: margin
	--
	-- The widget's margins in clockwise order starting from `left`.
	--
	-- Note that margins do not "collapse" like CSS margins.
	-- Padding is not provided; to simulate padding, add a container node with
	-- the requested padding as a margin and append children to it.
	margin = { 0, 0, 0, 0 },

	-- Property: anchor
	--
	-- Accepted Values: one of <AnchorMode>
	anchor = "Center",

	-- Property: dpiIndependent
	--
	-- Sizes and margins are automatically scaled by the display's DPI ratio
	-- unless this parameter is enabled. When `nil`, this value is inherited
	-- from the parent widget
	dpiIndependent = nil,

	-- These values are nil until an __update has been called (to allow code to sanely
	-- default to the calculated size or the initial size if none has been calculated).
	-- __pos = Vector2(),
	-- __extent = Vector2(),

	-- the layout function is called to build the box model when the widget hierarchy
	-- changes or an update is triggered. The context state is thrown away and generated
	-- from scratch (given that it's relatively cheap to do)
	__layout = function(self, context)
		-- TODO: dpi awareness!
		local itm = context:item()
		context:set_size(itm, self.size)
		context:set_margins(itm, self.margin)
		context:set_behavior(itm, self.anchor)
		self.__item = itm
	end,

	-- the update function is called after the layouting engine has been run, and serves
	-- to update the calculated positions of widgets
	__update = function(self, context)
		self.__pos, self.__extent = context:get_rect(self.__item)
	end,

	-- the draw function actually draws the widget using pigui routines
	__draw = function(self)
		-- the base widget implementation doesn't need to draw anything
		pigui.AddRectFilled(self.__pos, self.__pos + self.__extent, "#BBB", 0, 0)
	end
}

-- make sure the Base widget has the proper metatable set up
widgets.Base = clone(nil, widgets.Base)

-- Enum: LayoutMode
-- Controls the child-layout behavior of containers.
--
-- Free		- Children are positioned only by anchors
-- Row		- Children are arranged in rows, potentially wrapping horizontally
-- Column	- Children are arranged in columns, potentially wrapping vertically

-- Enum: JustifyMode
-- Controls position and spacing of child elements along the container axis.
-- This value has no effect if the container's layout mode is <LayoutMode.Free>.
--
-- Start	- Children are justified against the left side of the container
-- Center	- Children are placed in the center
-- End		- Children are justified against the right side of the container
-- Justify	- Space is inserted between each container

-- Class: Widgets.Layout
-- The base of all container widgets. Inherits from <Base>
widgets.Layout = widgets.Base {
	anchor = "Fill",

	-- Property: id
	--
	-- Add this ID to the ImGui internal ID stack for this container's children
	-- If the property is nil, no push/pop of ID takes place.
	id = nil,

	-- Property: layout
	--
	-- Accepted Values: one of <LayoutMode>.
	layout = "Free",

	-- Property: justify
	--
	-- Accepted Values: one of <JustifyMode>.
	justify = "Center",

	-- Property: wrap
	--
	-- Enable wrapping of child elements to new rows or columns.
	wrap = false,

	-- Property: spacing
	--
	-- When <layout> is <LayoutMode.Row> or <LayoutMode.Column>, this
	-- value controls the horizontal or vertical spacing between child elements.
	spacing = Vector2(8, 4),

	-- a container's layout implementation is concerned with dispatching
	-- to child widgets and ensuring the proper container behavior is set
	__layout = function(self, context)
		-- create the layout item and set needed properties
		widgets.Base.__layout(self, context)
		local itm = self.__item
		context:set_container(itm, self.layout .. "|" .. self.justify, self.wrap)

		-- create the internal children array
		self.__children = {}
		-- the array part of a container widget holds its children
		for k, v in ipairs(self) do
			-- if we've passed a child as a function, evaluate it now to get a widget
			if type(v) == "function" then
				v = v(self)
			end

			-- layout the child widget (including children)
			v.__root = self.__root
			if self.layout == "Column" and k > 1 then
				-- clone the table before updating, to avoid mangling more widgets
				v.margin = { table.unpack(v.margin) }
				v.margin[2] = math.max(v.margin[2], self.spacing.y)
			elseif self.layout == "Row" and k > 1 then
				-- clone the table before updating, to avoid mangling more widgets
				v.margin = { table.unpack(v.margin) }
				v.margin[1] = math.max(v.margin[1], self.spacing.x)
			end
			v:__layout(context)
			self.__children[k] = v
			context:insert(itm, v.__item)
		end
	end,

	__update = function(self, context)
		widgets.Base.__update(self, context)
		for k, v in ipairs(self.__children) do
			v:__update(context)
		end
	end,

	__draw = function(self)
		widgets.Base.__draw(self)
		if self.id then pigui.PushID(self.id) end
		for k, v in ipairs(self.__children) do
			v:__draw()
		end
		if self.id then pigui.PopID(self.id) end
	end
}

-- Class: Widgets.VBox
-- A vertical box container that lays out children in column order. Inherits from <Layout>
widgets.VBox = widgets.Layout {
	layout = "Column"
}

-- Class Widgets.HBox
-- A horizontal box container that lays out children in row order. Inherits from <Layout>
widgets.HBox = widgets.Layout {
	layout = "Row"
}

return widgets
