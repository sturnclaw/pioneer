-- Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt
local Comms = require 'Comms'
local Game = require 'Game'
local PlayerState = require 'PlayerState'

local Lang = require 'Lang'
local l = Lang.GetResource("ui-core")


--
-- Namespace: Legal
--

--
-- Constants: CrimeType
--
-- Different types of crimes and law offences
--
-- DUMPING - jettison of hazardous rubble/waste
-- MURDER - destruction of ship
-- PIRACY - fired on ship
-- TRADING_ILLEGAL_GOODS - attempted to sell illegal goods
-- WEAPONS_DISCHARGE - weapons discharged too close to station
-- ECM_DISCHARGE - ECM discharged too close to station
--
-- Availability:
--
--   2015 July
--
-- Status:
--
--   experimental
--


local Legal = {}
Legal.CrimeType = {}

Legal.CrimeType["DUMPING"] = {basefine = 1e4, name = l.DUMPING}
Legal.CrimeType["ILLEGAL_JUMP"] = {basefine = 5e2, name = l.ILLEGAL_JUMP}
Legal.CrimeType["MURDER"] = {basefine = 1.5e6, name = l.MURDER}
Legal.CrimeType["PIRACY"] = {basefine = 1e5, name = l.PIRACY}
Legal.CrimeType["TRADING_ILLEGAL_GOODS"] = {basefine = 5e3, name = l.TRADING_ILLEGAL_GOODS}
Legal.CrimeType["WEAPONS_DISCHARGE"] = {basefine = 5e2, name = l.UNLAWFUL_WEAPONS_DISCHARGE}
Legal.CrimeType["ECM_DISCHARGE"] = {basefine = 7.5e2, name = l.UNLAWFUL_ECM_DISCHARGE}
Legal.CrimeType["CONTRACT_FRAUD"] = {basefine = 5e2, name = l.CONTRACT_FRAUD}


function Legal:fine (crime, lawlessness)
	return math.max(1, 1+math.floor(self.CrimeType[crime].basefine * (1.0-lawlessness)))
end

function Legal:notifyOfCrime (ship, crime)
	if not ship:IsPlayer() then return end
	-- TODO: can this be called in hyperpace?

	-- find closest law enforcing station
	local station = Game.player:FindNearestTo("SPACESTATION")

	-- no plaintiff no judgement
	if station == nil then return end

	-- too far away for crime to be noticed
	if station.lawEnforcedRange < station:DistanceTo(Game.player) then
		return
	end

	Comms.ImportantMessage(string.interp(l.X_CANNOT_BE_TOLERATED_HERE, {crime=self.CrimeType[crime].name}),
						   station.label)

	local lawlessness = Game.system.lawlessness
	local _, outstandingfines = PlayerState.GetCrimeOutstanding()
	local newFine = self:fine(crime, lawlessness)

	-- don't keep compounding fines, except for murder
	if crime ~= "MURDER" and newFine < outstandingfines then newFine = 0 end
	PlayerState.AddCrime(crime, newFine)
end


return Legal
