//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef BACKGROUNDIMAGE_H
#define BACKGROUNDIMAGE_H

#ifdef _WIN32
#pragma once
#endif

#include "csshelpers.h"
#include "../data/panoramavideoplayer.h"
#include "utlstring.h"
#include "uilength.h"

namespace panorama
{

class IImageSource;
class CMovie;
class CPanel2D;


//-----------------------------------------------------------------------------
// Purpose: Background position for background image
//-----------------------------------------------------------------------------
class CBackgroundPosition
{
public:
	CBackgroundPosition();
	~CBackgroundPosition() {}

	
	EHorizontalAlignment GetHorizontalAlignment() const { return m_eHorizontalAlignment; }
	CUILength GetHorizontalLength() const { return m_horizontal; }
	EVerticalAlignment GetVeriticalAlignment() const { return m_eVerticalAlignment; }
	CUILength GetVerticalLength() const { return m_vertical; }

	bool IsHorizontalSet() const
	{
		return ( m_eHorizontalAlignment != k_EHorizontalAlignmentUnset && m_horizontal.IsSet() );
	}

	bool IsVerticalSet() const
	{
		return ( m_eVerticalAlignment != k_EVerticalAlignmentUnset && m_vertical.IsSet() );
	}

	bool IsSet() const
	{
		return ( IsHorizontalSet() && IsVerticalSet() );
	}

	void Set( EHorizontalAlignment eHorizontal, const CUILength &horizontal, EVerticalAlignment eVertical, const CUILength &vertical );
	void ResolveDefaultValues();
	void ToString( CFmtStr1024 *pfmtBuffer );
	
	void ScaleLengthValues( float flScaleFactorX, float flScaleFactorY )
	{
		m_horizontal.ScaleLengthValue( flScaleFactorX );
		m_vertical.ScaleLengthValue( flScaleFactorY );
	}

	bool operator==( const CBackgroundPosition &rhs ) const
	{
		return ( m_eHorizontalAlignment == rhs.m_eHorizontalAlignment && m_horizontal == rhs.m_horizontal && m_eVerticalAlignment == rhs.m_eVerticalAlignment && m_vertical == rhs.m_vertical );
	}

	bool operator!=( const CBackgroundPosition &rhs ) const
	{
		return !( *this == rhs );
	}

private:
	EHorizontalAlignment m_eHorizontalAlignment;
	CUILength m_horizontal;

	EVerticalAlignment m_eVerticalAlignment;
	CUILength m_vertical;
};


//-----------------------------------------------------------------------------
// Purpose: Background images have a repeat in the x & y directions
//-----------------------------------------------------------------------------
class CBackgroundRepeat
{
public:
	CBackgroundRepeat() { Set( k_EBackgroundRepeatUnset, k_EBackgroundRepeatUnset ); }
	~CBackgroundRepeat() {}

	bool IsSet() const { return (m_eHorizontal != k_EBackgroundRepeatUnset && m_eVertical != k_EBackgroundRepeatUnset); }
	void Set( EBackgroundRepeat eHorizontal, EBackgroundRepeat eVertical )
	{
		m_eHorizontal = eHorizontal;
		m_eVertical = eVertical;
	}

	void ResolveDefaultValues()
	{
		if ( m_eHorizontal == k_EBackgroundRepeatUnset )
			m_eHorizontal = k_EBackgroundRepeatRepeat;

		if ( m_eVertical == k_EBackgroundRepeatUnset )
			m_eVertical = k_EBackgroundRepeatRepeat;
	}

	EBackgroundRepeat GetHorizontal() const { return m_eHorizontal; }
	EBackgroundRepeat GetVertical() const { return m_eVertical; }

	bool operator==( const CBackgroundRepeat &rhs ) const
	{
		return ( m_eHorizontal == rhs.m_eHorizontal && m_eVertical == rhs.m_eVertical );
	}

	bool operator!=( const CBackgroundRepeat &rhs ) const
	{
		return !( *this == rhs );
	}

private:
	EBackgroundRepeat m_eHorizontal;
	EBackgroundRepeat m_eVertical;
};


//-----------------------------------------------------------------------------
// Purpose: Can have multiple background image layers, each contains the following
//-----------------------------------------------------------------------------
class CBackgroundImageLayer
{
private:
	enum EImagePath
	{
		k_EImagePathUnset,
		k_EImagePathNone,
		k_EImagePathSet
	};

public:

	CBackgroundImageLayer();
	~CBackgroundImageLayer();	

	void SetPathUnset()
	{
		m_eImagePath = k_EImagePathUnset;
		m_sURLPath.Clear();
	}

	void SetPathToNone()
	{
		m_eImagePath = k_EImagePathNone;
		m_sURLPath.Clear();
	}

	void SetPath( const char *pchPath )
	{
		m_eImagePath = k_EImagePathSet;
		m_sURLPath = pchPath;
	}

	bool IsPathSet() { return (m_eImagePath != k_EImagePathUnset); }
	bool IsPathNone() { return (m_eImagePath == k_EImagePathNone); }
	const char *GetPath() { return m_sURLPath.String(); }

	// from background-position
	const CBackgroundPosition &GetPosition() { return m_position; }
	void SetPosition( const CBackgroundPosition &position ) { m_position = position; }

	const CUILength &GetWidth() { return m_width; }
	const CUILength &GetHeight() { return m_height; }
	void SetBackgroundSize( const CUILength &width, const CUILength &height )
	{
		m_width = width;
		m_height = height;
		m_eBackgroundSizeConstant = k_EBackgroundSizeConstantNone;
	}

	EBackgroundSizeConstant GetBackgroundSizeConstant() { return m_eBackgroundSizeConstant; }
	void SetBackgroundSize( EBackgroundSizeConstant eConstant )
	{
		Assert( eConstant != k_EBackgroundSizeConstantNone );
		m_eBackgroundSizeConstant = eConstant;
		m_width.SetLength( k_flFloatAuto );
		m_height.SetLength( k_flFloatAuto );
	}

	CBackgroundRepeat GetRepeat() { return m_repeat; }
	void SetRepeat( const CBackgroundRepeat &repeat ) { m_repeat = repeat; }

	void SetOpacity( float flOpacity ) { m_flOpacity = flOpacity; }
	float GetOpacity() const { return m_flOpacity; }
	float GetInterpolatedOpacity( float flProgress ) const;

	void SetTemporaryLayer( bool bTemporaryLayer ) { m_bTemporaryLayer = bTemporaryLayer; }
	bool IsTemporaryLayer() const { return m_bTemporaryLayer; }

	IImageSource *GetImage() { return m_pImage; }
	CVideoPlayerPtr GetMovie() { return m_pVideoPlayer; }

	bool IsCompletelyUnset()
	{
		return (m_eImagePath == k_EImagePathUnset && !m_position.IsSet() && !m_width.IsSet() && !m_height.IsSet() && !m_repeat.IsSet() );
	}

	bool IsSet()
	{
		return (m_eImagePath != k_EImagePathUnset && m_position.IsSet() && m_width.IsSet() && m_height.IsSet() && m_repeat.IsSet() );
	}

	void ResolveDefaultValues();
	void ApplyUIScaleFactor( float flScaleFactorX, float flScaleFactorY );
	void OnAppliedToPanel( IUIPanel *pPanel );
	void MergeTo( CBackgroundImageLayer *pTarget );
	void ToString( CFmtStr1024 *pfmtBuffer );

	// comparison operators
	bool operator==( const CBackgroundImageLayer &rhs ) const
	{
		return ( m_eImagePath == rhs.m_eImagePath && m_sURLPath == rhs.m_sURLPath && m_position == rhs.m_position && 
			m_width == rhs.m_width&& m_height == rhs.m_height && m_repeat == rhs.m_repeat && m_eBackgroundSizeConstant == rhs.m_eBackgroundSizeConstant );
	}

	bool operator!=( const CBackgroundImageLayer &rhs ) const
	{
		return !( *this == rhs );
	}

	// helpers for drawing
	void CalculateFinalDimensions( float *pflWidth, float *pflHeight, float flPanelWidth, float flPanelHeight, float flScaleFactorX, float flScaleFactorY );
	void CalculateFinalPosition( float *px, float *py, float flWidthPanel, float flHeightPanel, float flWidthImage, float flHeightImage );
	void CalculateFinalSpacing( float *px, float *py, float flWidthPanel, float flHeightPanel, float flWidthImage, float flHeightImage );

	void Set( const CBackgroundImageLayer &rhs );
	void UnloadImage();
	void ReloadImage( IUIPanel *pPanel );


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	CBackgroundImageLayer( const CBackgroundImageLayer &rhs );	
	CBackgroundImageLayer& operator=( const CBackgroundImageLayer &rhs ) const;

	// from background-image
	CUtlString m_sURLPath;
	EImagePath m_eImagePath;

	// from background-position
	CBackgroundPosition m_position;

	// from background-size
	EBackgroundSizeConstant m_eBackgroundSizeConstant;
	CUILength m_width;
	CUILength m_height;

	// from background-repeat
	CBackgroundRepeat m_repeat;

	// Used when crossfading. Not considered for equality purposes
	float m_flOpacity;
	bool m_bTemporaryLayer;

	// loaded when applied to a panel
	IImageSource *m_pImage;
	CVideoPlayerPtr m_pVideoPlayer;
};

} // namespace panorama

#endif //BACKGROUNDIMAGE_H
