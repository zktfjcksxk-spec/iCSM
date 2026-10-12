//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//

#include "stdafx.h"
#include "panorama/renderer/rendercommands.h"
#include "memstack.h"

namespace panorama
{


const char *GetRenderCommandTypeName( ERenderCommand eCommandType )
{
	switch ( eCommandType )
	{
		case k_EBeginFrame:							return "k_EBeginFrame";
		case k_EEndFrame:							return "k_EEndFrame";
		case k_EClearBackBuffer:					return "k_EClearBackBuffer";
		//case k_ESetRenderEffect:					return "k_ESetRenderEffect";
		case k_ECmdDrawTexturedRect:				return "k_ECmdDrawTexturedRect";
		case k_EPushAnimationAndTransformContext:	return "k_EPushAnimationAndTransformContext";
		case k_EPopAnimationAndTransformContext:	return "k_EPopAnimationAndTransformContext";
		case k_EPushCompositingLayer:				return "k_EPushCompositingLayer";
		case k_EPopCompositingLayer:				return "k_EPopCompositingLayer";
		case k_EDrawFilledRect:						return "k_EDrawFilledRect";
		case k_EDrawTextRegion:						return "k_EDrawTextRegion";
		//case k_ELoadOpacityMaskA8:				return "k_ELoadOpacityMaskA8";
		case k_EBeginPaintBackground:				return "k_EBeginPaintBackground";
		case k_EEndPaintBackground:					return "k_EEndPaintBackground";
		case k_EPushClipLayer:						return "k_EPushClipLayer";
		case k_EPopClipLayer:						return "k_EPopClipLayer";
		case k_EBeginPaintLast:						return "k_EBeginPaintLast";
		case k_EEndPaintLast:						return "k_EEndPaintLast";
		//case k_EDeleteTexture:						return "k_EDeleteTexture";
		case k_EDeleteParticleSystem:				return "k_EDeleteParticleSystem";
		case k_ECmdFreeCompositingLayer:			return "k_ECmdFreeCompositingLayer";
		case k_ECmdLockTexture:						return "k_ECmdLockTexture";
		case k_EDeletePanel:						return "k_EDeletePanel";
		case k_ERequestRenderCallback:				return "k_ERequestRenderCallback";
		case k_EPushPanelContextInLayer:			return "k_EPushPanelContextInLayer";
		case k_EPopPanelContextInLayer:				return "k_EPopPanelContextInLayer";
		case k_ENestedCommand:						return "k_ENestedCommand";
		case k_ENestedCommandList:					return "k_ENestedCommandList";

		default:
		{
			static char s_szBuffer[ 64 ];
			V_sprintf_safe( s_szBuffer, "Unknown(%d)", ( int )eCommandType );
			return s_szBuffer;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRenderCommandList::CRenderCommandList()
	: m_pFirstCommand( nullptr )
	, m_pLastCommand( nullptr )
	, m_unTotalBytesAllocated( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRenderCommandList::~CRenderCommandList()
{
	for ( CMemoryStack *pMemoryStack : m_vecMemoryStacks )
	{
		UIEngineInternal()->ReleaseMemoryCommandStack( pMemoryStack );
	}

	for ( byte *pOversizedAlloc : m_vecOversizedAllocs )
	{
		delete[] pOversizedAlloc;
	}

	for ( IUITexture *pTexture : m_vecTextures )
	{
		pTexture->Release();
	}

	for ( CRefCount *pObject : m_vecRefCountObjs )
	{
		pObject->Release();
	}

	for ( CRefCount *pObject : m_vecRefCountObjsDelayedRelease )
	{
		UIEngine()->QueueDecrementRefNextFrame( pObject );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMemoryStack *CRenderCommandList::AcquireMemoryStack()
{
	return UIEngineInternal()->AcquireRenderCommandMemoryStack();
}


//-----------------------------------------------------------------------------
// Purpose: Hold a reference to this texture
//-----------------------------------------------------------------------------
void CRenderCommandList::AddTextureReference( IUITexture *pTexture )
{
	if ( !pTexture )
		return;

	pTexture->AddRef();
	m_vecTextures.AddToTail( pTexture );
}


//-----------------------------------------------------------------------------
// Purpose: Hold a reference to this object
//-----------------------------------------------------------------------------
void CRenderCommandList::AddObjectReference( CRefCount *pObject )
{
	if ( !pObject )
		return;

	pObject->AddRef();
	m_vecRefCountObjs.AddToTail( pObject );
}


//-----------------------------------------------------------------------------
// Purpose: Hold a reference to this object, and when the list is destroyed,
// release the reference the frame afterwards.
//-----------------------------------------------------------------------------
void CRenderCommandList::AddObjectReferenceDelayedRelease( CRefCount *pObject )
{
	if ( !pObject )
		return;

	pObject->AddRef();
	m_vecRefCountObjsDelayedRelease.AddToTail( pObject );
}


//-----------------------------------------------------------------------------
// Purpose: Take a reference to each ref counted object in the other command list
//-----------------------------------------------------------------------------
void CRenderCommandList::CopyObjectReferences( CRenderCommandList &other )
{
	for ( CRefCount *pObject : other.m_vecRefCountObjs )
	{
		pObject->AddRef();
		m_vecRefCountObjs.AddToTail( pObject );
	}

	for ( CRefCount *pObject : other.m_vecRefCountObjsDelayedRelease )
	{
		pObject->AddRef();
		m_vecRefCountObjsDelayedRelease.AddToTail( pObject );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set this texture, and mark the command list as holding a reference so it
//-----------------------------------------------------------------------------
void CRenderCommandTexture::SetTexture( IUITexture *pTexture, CRenderCommandList &commandList )
{
	m_pTexture = pTexture;
	commandList.AddTextureReference( pTexture );
}


//-----------------------------------------------------------------------------
// Purpose: Utility method to convert a text format and text ranges into a cache key
//-----------------------------------------------------------------------------
void RenderCommandToTextLayoutKey( const RenderTextFormat_t &defaultFormat, const CRenderDataList< RenderTextRangeFormat_t > &rangeFormats, UITextLayoutProperties_t *pKey )
{
	// copy default
	uint32 unCRCFontNameDefault = CRC32_ProcessSingleBuffer( defaultFormat.font_name, V_strlen( defaultFormat.font_name ) );
	pKey->m_defaultFormat.m_unCharStartIndex = 0;
	pKey->m_defaultFormat.m_unCharEndIndex = 0;
	pKey->m_defaultFormat.m_unCRCFontName = unCRCFontNameDefault;
	pKey->m_defaultFormat.m_flFontSize = defaultFormat.font_size;
	pKey->m_defaultFormat.m_eWeight = defaultFormat.font_weight;
	pKey->m_defaultFormat.m_eStyle = defaultFormat.font_style;
	pKey->m_defaultFormat.m_bUnderline = defaultFormat.text_decoration == k_ETextDecorationUnderline;
	pKey->m_defaultFormat.m_bStrikethrough = defaultFormat.text_decoration == k_ETextDecorationLineThrough;
	pKey->m_defaultFormat.m_iLetterSpacing = defaultFormat.letter_spacing;
	pKey->m_defaultFormat.m_iColorIndex = UITextFormatProperties_t::k_iColorIndexUnset;
	pKey->m_defaultFormat.m_flInlineObjectWidth = defaultFormat.inline_object ? defaultFormat.inline_object->width : 0.0f;
	pKey->m_defaultFormat.m_flInlineObjectHeight = defaultFormat.inline_object ? defaultFormat.inline_object->height : 0.0f;

	// copy ranges
	int nRangeFormatsCount = 0;
	for ( const RenderTextRangeFormat_t *pTextRangeFormat : rangeFormats )
	{
		NOTE_UNUSED( pTextRangeFormat ); // not actually using this - just counting
		++nRangeFormatsCount;
	}

	if ( nRangeFormatsCount > 0 )
	{
		pKey->m_arrayFormats.Allocate( nRangeFormatsCount );

		int i = 0;
		for ( const RenderTextRangeFormat_t *pRangeFormat : rangeFormats )
		{
			const RenderTextFormat_t &format = pRangeFormat->format;

			// when copying range data, if a value is not set, use default
			UITextFormatProperties_t &formatKey = pKey->m_arrayFormats[ i ];
			formatKey.m_unCharStartIndex = pRangeFormat->start_index;
			formatKey.m_unCharEndIndex = pRangeFormat->end_index;
			formatKey.m_unCRCFontName = format.font_name ? CRC32_ProcessSingleBuffer( format.font_name, V_strlen( format.font_name ) ) : unCRCFontNameDefault;
			formatKey.m_flFontSize = format.font_size > 0.0f ? format.font_size : defaultFormat.font_size;
			formatKey.m_eWeight = format.font_weight != k_EFontWeightUnset ? format.font_weight : defaultFormat.font_weight;
			formatKey.m_eStyle = format.font_style != k_EFontStyleUnset ? format.font_style : defaultFormat.font_style;
			formatKey.m_bUnderline = format.text_decoration != k_ETextDecorationUnset ? format.text_decoration == k_ETextDecorationUnderline : defaultFormat.text_decoration == k_ETextDecorationUnderline;
			formatKey.m_bStrikethrough = format.text_decoration != k_ETextDecorationUnset ? format.text_decoration == k_ETextDecorationLineThrough : defaultFormat.text_decoration == k_ETextDecorationLineThrough;
			formatKey.m_iColorIndex = !format.fill_brush_collection.fill_brush.IsEmpty() ? i : UITextFormatProperties_t::k_iColorIndexUnset;
			formatKey.m_flInlineObjectWidth = format.inline_object ? format.inline_object->width : 0.0f;
			formatKey.m_flInlineObjectHeight = format.inline_object ? format.inline_object->height : 0.0f;
			formatKey.m_iLetterSpacing = 0; // Not used for range formats

			++i;
		}
	}
}

void RenderCommandToTextLayoutKey( const TextFormat_t &defaultFormat, const CRenderDataList< TextRangeFormatData_t > &rangeFormats, UITextLayoutProperties_t *pKey )
{
	// copy default
	uint32 unCRCFontNameDefault = CRC32_ProcessSingleBuffer( defaultFormat.font_name, V_strlen( defaultFormat.font_name ) );
	pKey->m_defaultFormat.m_unCharStartIndex = 0;
	pKey->m_defaultFormat.m_unCharEndIndex = 0;
	pKey->m_defaultFormat.m_unCRCFontName = unCRCFontNameDefault;
	pKey->m_defaultFormat.m_flFontSize = defaultFormat.font_size;
	pKey->m_defaultFormat.m_eWeight = defaultFormat.font_weight;
	pKey->m_defaultFormat.m_eStyle = defaultFormat.font_style;
	pKey->m_defaultFormat.m_bUnderline = defaultFormat.text_decoration == k_ETextDecorationUnderline;
	pKey->m_defaultFormat.m_bStrikethrough = defaultFormat.text_decoration == k_ETextDecorationLineThrough;
	pKey->m_defaultFormat.m_iLetterSpacing = defaultFormat.letter_spacing;
	pKey->m_defaultFormat.m_iColorIndex = UITextFormatProperties_t::k_iColorIndexUnset;
	pKey->m_defaultFormat.m_flInlineObjectWidth = defaultFormat.inline_object ? defaultFormat.inline_object->width : 0.0f;
	pKey->m_defaultFormat.m_flInlineObjectHeight = defaultFormat.inline_object ? defaultFormat.inline_object->height : 0.0f;

	// copy ranges
	int nRangeFormatsCount = 0;
	for( const TextRangeFormatData_t *pTextRangeFormat : rangeFormats )
	{
		NOTE_UNUSED( pTextRangeFormat ); // not actually using this - just counting
		++nRangeFormatsCount;
	}

	if( nRangeFormatsCount > 0 )
	{
		pKey->m_arrayFormats.Allocate( nRangeFormatsCount );

		int i = 0;
		for( const TextRangeFormatData_t *pRangeFormat : rangeFormats )
		{
			const TextFormat_t &format = pRangeFormat->format;

			// when copying range data, if a value is not set, use default
			UITextFormatProperties_t &formatKey = pKey->m_arrayFormats[i];
			formatKey.m_unCharStartIndex = pRangeFormat->start_index;
			formatKey.m_unCharEndIndex = pRangeFormat->end_index;
			formatKey.m_unCRCFontName = format.font_name ? CRC32_ProcessSingleBuffer( format.font_name, V_strlen( format.font_name ) ) : unCRCFontNameDefault;
			formatKey.m_flFontSize = format.font_size > 0.0f ? format.font_size : defaultFormat.font_size;
			formatKey.m_eWeight = format.font_weight != k_EFontWeightUnset ? format.font_weight : defaultFormat.font_weight;
			formatKey.m_eStyle = format.font_style != k_EFontStyleUnset ? format.font_style : defaultFormat.font_style;
			formatKey.m_bUnderline = format.text_decoration != k_ETextDecorationUnset ? format.text_decoration == k_ETextDecorationUnderline : defaultFormat.text_decoration == k_ETextDecorationUnderline;
			formatKey.m_bStrikethrough = format.text_decoration != k_ETextDecorationUnset ? format.text_decoration == k_ETextDecorationLineThrough : defaultFormat.text_decoration == k_ETextDecorationLineThrough;
			formatKey.m_iColorIndex = !format.fill_brush_collection.base.fill_brush.IsEmpty() ? i : UITextFormatProperties_t::k_iColorIndexUnset;
			formatKey.m_flInlineObjectWidth = format.inline_object ? format.inline_object->width : 0.0f;
			formatKey.m_flInlineObjectHeight = format.inline_object ? format.inline_object->height : 0.0f;
			formatKey.m_iLetterSpacing = 0; // Not used for range formats

			++i;
		}
	}
}


} // panorama
