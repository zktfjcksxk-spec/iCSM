#ifndef INCLUDED_VERTEXDATA_H
#define INCLUDED_VERTEXDATA_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "tier0/platform.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"

enum VertexDataStrideType_t
{
	VD_STRIDE_ZERO = 0,
	VD_STRIDE_DEFAULT,
};

template< class T > class ALIGN16 CDynamicVertexData
{
public:
//	CVertexData( IRenderContext* pRenderContext, HRenderBuffer hVertexBuffer );
	CDynamicVertexData( IRenderContext* pRenderContext, int nVertexCount, const char *pDebugName, const char *pBudgetGroup );
	~CDynamicVertexData();

	CUtlVectorFixed<T, 1024>	m_Verts;
	uint32			m_nVerts;

	FORCEINLINE T* operator->() { return &m_Verts.Tail(); }

	void AdvanceVertex()
	{
		m_Verts.AddToTail();
	}

	void Reset()
	{
		m_Verts.SetCount( 1 );
	}

	void Unlock()
	{
	}

	void Bind( int nSlot, int nOffset = 0, VertexDataStrideType_t nStride = VD_STRIDE_DEFAULT )
	{
	}

	FORCEINLINE void UnlockAndBind( int nSlot, int nOffset = 0, VertexDataStrideType_t nStride = VD_STRIDE_DEFAULT )
	{
		m_pContext->m_pBaseVB = (void*)m_Verts.Base();
		m_pContext->m_nVertCount = m_Verts.Count() - 1;
		Unlock();
		Bind( nSlot, nOffset, nStride );
	}


	CRenderContext* m_pContext;

};



template< class T >
inline CDynamicVertexData<T>::CDynamicVertexData( IRenderContext* pRenderContext, int nVertexCount, const char *pDebugName, const char *pBudgetGroup )
{
	m_pContext = (CRenderContext*)pRenderContext;

	m_Verts.AddToTail();
}

template< class T >
inline CDynamicVertexData<T>::~CDynamicVertexData()
{


}



#endif // INCLUDED_VERTEXDATA_H