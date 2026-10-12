//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_SCROLLBAR_H
#define PANORAMA_SCROLLBAR_H

#ifdef _WIN32
#pragma once
#endif


#include "../iuipanel.h"
#include "../iuipanelclient.h"
#include "panel2d.h"

namespace panorama
{
//-----------------------------------------------------------------------------
// Purpose: Base class for all types of scroll bars
//-----------------------------------------------------------------------------
class CBaseScrollBar : public CPanel2D, public IUIScrollBar
{
	DECLARE_PANEL2D( CBaseScrollBar, CPanel2D );

public:
	CBaseScrollBar( CPanel2D *parent, const char * pchPanelID );

	virtual ~CBaseScrollBar();

	virtual IUIPanel* UIPanel() OVERRIDE { return BaseClass::UIPanel(); }
	virtual IUIPanelClient* ClientPtr() OVERRIDE { return UIPanel()->ClientPtr(); }

	// Normalizes the position to be within range min/max 
	virtual void Normalize( bool bImmediateThumbUpdate = false, bool bUpdateLayout = true ) OVERRIDE
	{
		if ( m_flWindowStart < m_flRangeMin )
			m_flWindowStart = m_flRangeMin;

		if ( m_flWindowStart + m_flWindowSize > m_flRangeMax )
		{
			m_flWindowStart = m_flRangeMax - m_flWindowSize;
			m_flWindowStart = RoundFloatToInt( m_flWindowStart );
		}
		else
		{
			m_flWindowStart = RoundFloatToInt( m_flWindowStart );
		}

		if ( bUpdateLayout )
		{
			UpdateLayout( bImmediateThumbUpdate );
		}
	}

	// Set the scroll range 
	virtual void SetRangeMinMax( float flRangeMin, float flRangeMax ) OVERRIDE
	{
		if ( m_flRangeMin != flRangeMin || m_flRangeMax != flRangeMax )
		{
			m_flRangeMin = flRangeMin;
			m_flRangeMax = flRangeMax;

			if ( GetParent() )
				GetParent()->InvalidatePosition();

			Normalize( false );
		}
	}

	// return the size of the range we have
	virtual float GetRangeSize() const OVERRIDE { return m_flRangeMax - m_flRangeMin; }
	virtual float GetRangeMin() const OVERRIDE { return m_flRangeMin; }
	virtual float GetRangeMax() const OVERRIDE { return m_flRangeMax; }

	// Set the window size
	virtual void SetScrollWindowSize( float flWindowSize ) OVERRIDE
	{
		if ( m_flWindowSize != flWindowSize )
		{
			m_flWindowSize = flWindowSize;

			if ( GetParent() )
				GetParent()->InvalidatePosition();

			Normalize( false );
		}
	}

	// Get scroll window size
	virtual float GetScrollWindowSize() OVERRIDE { return m_flWindowSize; }

	// Set the current window position
	virtual void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove = false ) OVERRIDE { SetScrollWindowPosition( flWindowPos, bImmediateMove, true ); }
	void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove, bool bEndFlick );

	virtual float GetLastScrollTime() OVERRIDE { return m_flLastScrollTime; }

	// Get scroll window position
	virtual float GetScrollWindowPosition() OVERRIDE { return m_flWindowStart; }

	// Get scroll window position, accounting for a scroll in progress
	virtual float GetInterpolatedScrollWindowPosition() = 0;

	virtual bool BLastMoveImmediate() OVERRIDE { return m_bLastMoveImmediate; }

	virtual EAnimationTimingFunction GetTransitionTimingFunction() OVERRIDE
	{
		if ( m_eTimingFunction == k_EAnimationNone )
		{
			LoadTransitionData();
		}
		return m_eTimingFunction;
	}
	
	virtual double GetTransitionDuration() OVERRIDE
	{
		if ( m_flTransitionTime == 0.0 )
		{
			LoadTransitionData();
		}
		return m_flTransitionTime; 
	}

	virtual void GetTransitionControlPoints( Vector2D (&vecPoints)[4] ) OVERRIDE
	{
		for ( int i = 0; i < 4; i++ )
		{
			vecPoints[i].x = m_vecControlPoints[i].x;
			vecPoints[i].y = m_vecControlPoints[i].y;
		}
	}

	void LoadTransitionData();

	virtual void OnStylesChanged() OVERRIDE
	{
		m_eTimingFunction = k_EAnimationNone;
		m_flTransitionTime = 0.0;
	}

	void StartFlick();
	void EndFlick();

	virtual bool OnDragScrollStart() OVERRIDE;
	virtual bool OnDragScrollMouseMove( int nLastPosition, int nCurrentPosition ) OVERRIDE;
	virtual bool OnDragScrollEnd( int nLastPosition, float flVelocity ) OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE
	{
		BaseClass::ValidateClientPanel( validator, pchName );
	}
#endif
protected:

	bool EventScrollFlickTimeout( const CPanelPtr< IUIPanel > &ptrPanel );
	virtual void StopScroll() = 0;

	virtual void UpdateLayout( bool bImmediateMove ) = 0;

	bool m_bLastMoveImmediate;

	double m_flLastScrollTime;
	float m_flRangeMin;
	float m_flRangeMax;
	float m_flWindowSize;
	float m_flWindowStart;

	EAnimationTimingFunction m_eTimingFunction;
	double m_flTransitionTime;
	Vector2D m_vecControlPoints[4];

	float m_flDragScrollFlickTime;
};

//-----------------------------------------------------------------------------
// Purpose: Default Scrollbar
//-----------------------------------------------------------------------------
class CScrollBar : public CBaseScrollBar
{
	DECLARE_PANEL2D( CScrollBar, CBaseScrollBar );

public:
	CScrollBar( CPanel2D *parent, const char * pchPanelID );
	virtual ~CScrollBar();

	void ScrollToMousePos()
	{
		CPanel2D *pPanel = GetParent();
		if ( pPanel )
		{
			float flHeight = GetActualLayoutLength();
			if ( flHeight > 0.00001f )
			{
				if ( m_bMouseWentDownOnThumb )
				{
					float flPercentDiff = (m_flMouseCoord - m_flMouseDownCoord) / flHeight;
					float flPositionOffset = flPercentDiff * GetContentLength();
					float flPosition = m_flScrollStartPosition + flPositionOffset;
					SetScrollWindowPosition( clamp( flPosition, 0.0f, GetContentLength() - GetScrollWindowSize() ), true );
				}
				else
				{
					float flPercent = m_flMouseCoord / flHeight;
					float flPos = GetContentLength() * flPercent;
					SetScrollWindowPosition( clamp( flPos, 0.0f, GetContentLength() - GetScrollWindowSize() ), true );
				}
			}
		}
	}
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE
	{
		// If there's a transform set on the parent, adjust the input as appropriate.
		Vector vMousePosition( flMouseX, flMouseY, 0.0f );
		if ( GetParent() )
		{
			VMatrix matParentTransform = GetParent()->AccessStyle()->GetTransform3DMatrix();
			vMousePosition = matParentTransform * vMousePosition;
		}

		SetMouseCoord( vMousePosition );

		if ( m_bMouseDown )
			ScrollToMousePos();
	}

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE
	{
		// Only interested in left clicks
		if ( code.m_MouseCode != MOUSE_LEFT )
			return BaseClass::OnMouseButtonDown( code );

		AddClass( "MouseDown" );
		
		if ( m_pScrollThumb->BHasHoverStyle() )
			m_bMouseWentDownOnThumb = true;
		else
			m_bMouseWentDownOnThumb = false;
		m_bMouseDown = true;
		m_flMouseDownCoord = m_flMouseCoord;
		m_flScrollStartPosition = GetScrollWindowPosition();
		ScrollToMousePos();

		return true;
	}

	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE
	{
		// Only interested in left clicks
		if ( code.m_MouseCode != MOUSE_LEFT )
			return BaseClass::OnMouseButtonDown( code );

		m_bMouseDown = false;
		ScrollToMousePos();

		RemoveClass( "MouseDown" );

		return true;
	}

protected:
	virtual float GetActualLayoutLength() = 0;
	virtual float GetContentLength() = 0;
	virtual void SetMouseCoord( const Vector &vMousePosition ) = 0;


	CPanel2D *m_pScrollThumb;

	bool m_bMouseDown;
	bool m_bMouseWentDownOnThumb;

	float m_flMouseCoord;
	float m_flMouseDownCoord;
	float m_flScrollStartPosition;
};


//-----------------------------------------------------------------------------
// Purpose: Vertical scroll bar
//-----------------------------------------------------------------------------
class CVerticalScrollBar : public CScrollBar
{
	DECLARE_PANEL2D( CVerticalScrollBar, CScrollBar );

public:
	CVerticalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID ) 
	{
		m_pScrollThumb->AddClass( "VerticalScrollThumb" );
	}
	
	virtual float GetInterpolatedScrollWindowPosition() OVERRIDE { return -GetParent()->GetInterpolatedYScrollOffset(); }

	virtual ~CVerticalScrollBar() {}

protected:
	virtual void UpdateLayout( bool bImmediateMove ) OVERRIDE;

	virtual float GetActualLayoutLength() OVERRIDE { return GetActualLayoutHeight(); }
	virtual float GetContentLength() OVERRIDE { return GetParent()->GetContentHeight(); }
	virtual void SetMouseCoord( const Vector &vMousePosition ) OVERRIDE { m_flMouseCoord = vMousePosition.y; }

	virtual void StopScroll() OVERRIDE;
};


//-----------------------------------------------------------------------------
// Purpose: Horizontal scroll bar
//-----------------------------------------------------------------------------
class CHorizontalScrollBar : public CScrollBar
{
	DECLARE_PANEL2D( CHorizontalScrollBar, CScrollBar );

public:
	CHorizontalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID ) 
	{
		m_pScrollThumb->AddClass( "HorizontalScrollThumb" );
	}

	virtual float GetInterpolatedScrollWindowPosition() OVERRIDE { return -GetParent()->GetInterpolatedXScrollOffset(); }

	virtual ~CHorizontalScrollBar() {}

protected:
	virtual void UpdateLayout( bool bImmediateMove ) OVERRIDE;

	virtual float GetActualLayoutLength() OVERRIDE { return GetActualLayoutWidth(); }
	virtual float GetContentLength() OVERRIDE { return GetParent()->GetContentWidth(); }
	virtual void SetMouseCoord( const Vector &vMousePosition ) OVERRIDE { m_flMouseCoord = vMousePosition.x; }

	virtual void StopScroll() OVERRIDE;
};

} // namespace panorama

#endif // PANORAMA_SCROLLBAR_H
 
