// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include <SDL2/SDL_net.h>

class HeadtrackingManager {
public:
	// opentrack UDP protocol
	struct State {
		// XYZ translation coordinates in meters
		double x, y, z;
		// yaw pitch roll in degrees
		double yaw, pitch, roll;
	};

public:
	HeadtrackingManager();
	~HeadtrackingManager();

	bool Connect(const char *host, uint16_t port);
	void Disconnect();

	const State *GetHeadState() const;

	void Update();

private:
	SDLNet_SocketSet m_socketSet;
	UDPsocket m_trackerSocket;
	UDPpacket* m_trackerPacket;
	State m_trackerState;
	bool m_connected;
};
