//===== Copyright © Valve Corporation, All rights reserved. ======//
#pragma once
struct EnginePVSContext_t
{
public:
	EnginePVSContext_t()
	{
		m_nFatBytes = 0;
		m_pFatPVS = NULL;
		m_pAreaFloodIndices = NULL;
		m_bOwnPortalOpen = false;
	}
	int		m_nFatBytes;
	byte*	m_pFatPVS;
	CUtlVector<int> m_ClustersNetworked;
	CUtlVector<int> m_AreasNetworked;

	bool m_bOwnPortalOpen; // this is true when the portal/area state is diverged in this context from the global context of the game; used for multithreading
	CUtlMemory< uint32 > m_PortalStateMemory;
	uint32 *GetPortalsOpenBits() { return m_PortalStateMemory.Base(); }
	uint16 *m_pAreaFloodIndices;
};
