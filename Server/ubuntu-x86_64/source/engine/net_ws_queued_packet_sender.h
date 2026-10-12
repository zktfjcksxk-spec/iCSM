//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_QUEUED_PACKET_SENDER_H
#define NET_WS_QUEUED_PACKET_SENDER_H
#ifdef _WIN32
#pragma once
#endif

#include <steamnetworkingsockets/steamnetworkingtypes.h>

class INetChannel;

class IQueuedPacketSender
{
public:
	virtual bool Setup() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsRunning() = 0;
	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan ) =  0;
	virtual void QueuePacketToSteamNetConnection( INetChannel *pChan, HSteamNetConnection hConn, const void *buf, int len, uint32 msecDelay ) = 0;
	virtual void QueuePacketToNetAdr( INetChannel *pChan, const netadr_t &to, SOCKET s, const void *buf, int len, uint32 msecDelay ) = 0;
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const = 0;
};

extern IQueuedPacketSender *g_pQueuedPackedSender;

#endif // NET_WS_QUEUED_PACKET_SENDER_H
