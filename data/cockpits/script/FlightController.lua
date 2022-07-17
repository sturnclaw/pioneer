-- Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local Input = require 'Input'
local Engine = require 'Engine'

-- FlightController implements the primary interaction method for cockpit
-- props and modules to talk to the ship and the world at large.

local fc = {}

-- Trigger the given input action if it exists
function fc:Action(actionId)
	print('Toggled action', actionId)
	local action = Input.GetActionBinding(actionId)

	if action then
		action:OnPress()
		action:OnRelease()
		Engine.pigui.PlaySfx("Click", 0.2)
	end
end

return fc
