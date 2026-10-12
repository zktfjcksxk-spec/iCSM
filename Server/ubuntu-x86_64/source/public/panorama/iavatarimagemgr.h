//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IAVATARIMAGEMGR_H
#define IAVATARIMAGEMGR_H
#pragma once

//-----------------------------------------------------------------------------
// Purpose: Avatar image manager. Functionality to associate a particular 
// image source with a steam ID
//-----------------------------------------------------------------------------

// Caller must free( *ppRGBA )
typedef bool (*AvatarImageBitsProvider)( uint64 xuid, unsigned char **ppRGBA, uint32 *pWidth, uint32 *pHeight );

class IAvatarImageMgr
{
public:
	virtual void SetImageBitsProvider( uint32 accountID, AvatarImageBitsProvider pfnBitsProvider ) = 0;
	virtual void Cleanup() = 0;

};

#endif // IAVATARIMAGEMGR_H
