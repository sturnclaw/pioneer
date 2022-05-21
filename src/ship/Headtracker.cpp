// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Headtracker.h"
#include "core/Log.h"

#include <SDL2/SDL_net.h>
#include <cstdint>

HeadtrackingManager::HeadtrackingManager() :
	m_socketSet(nullptr),
	m_trackerSocket(nullptr),
	m_trackerPacket(nullptr),
	m_trackerState {},
	m_connected(false)
{ }

HeadtrackingManager::~HeadtrackingManager()
{
	if (m_connected)
		Disconnect();
}

bool HeadtrackingManager::Connect(const char *host, uint16_t port)
{
	m_trackerSocket = SDLNet_UDP_Open(port);
	if (!m_trackerSocket) {
		Log::Error("SDLNet_UDP_Open error: {}\n", SDLNet_GetError());
		return false;
	}

	m_trackerPacket = SDLNet_AllocPacket(sizeof(State));
	if (!m_trackerPacket) {
		Log::Error("SDLNet_AllocPacket error: {}\n", SDLNet_GetError());
		return false;
	}

	m_socketSet = SDLNet_AllocSocketSet(1);
	if (!m_socketSet) {
		Log::Error("SDLNet_AllocSocketSet error: {}\n", SDLNet_GetError());
		return false;
	}

	int ret = SDLNet_UDP_AddSocket(m_socketSet, m_trackerSocket);
	if (ret == -1) {
		Log::Error("SDLNet_UDP_AddSocket error: {}\n", SDLNet_GetError());
		return false;
	}

	m_connected = true;
	return true;
}

void HeadtrackingManager::Disconnect()
{
	SDLNet_FreeSocketSet(m_socketSet);
	m_socketSet = nullptr;

	SDLNet_FreePacket(m_trackerPacket);
	m_trackerPacket = nullptr;

	SDLNet_UDP_Close(m_trackerSocket);
	m_trackerSocket = nullptr;

	m_connected = false;
}

const HeadtrackingManager::State *HeadtrackingManager::GetHeadState() const
{
	return &m_trackerState;
}

void HeadtrackingManager::Update()
{
	if (!m_connected)
		return;

	int numReady = SDLNet_CheckSockets(m_socketSet, 0);
	if (numReady <= 0)
		return;

	int numPackets = SDLNet_UDP_Recv(m_trackerSocket, m_trackerPacket);
	while (numPackets > 0) {
		// Packet is the wrong size for headtracking, ignore it
		if (m_trackerPacket->len != sizeof(State))
			continue;

		memcpy(&m_trackerState, m_trackerPacket->data, sizeof(State));
		numPackets = SDLNet_UDP_Recv(m_trackerSocket, m_trackerPacket);
	}
}
