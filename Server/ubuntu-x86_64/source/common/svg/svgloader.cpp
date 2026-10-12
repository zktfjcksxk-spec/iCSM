//========= Copyright (c) Valve Corporation, All rights reserved. ============
//
// Purpose: Load svg vector graphics from a file and convert to texture
//
//=============================================================================

#include "svgloader.h"

#include "../public/parsifal/parsifal.h"
#include "utlstring.h"
#include "utlmap.h"

#include "cairo.h"

#define SVG_PATH_SEGMENT_LARGE_ARC_FLAG	(1<<0)
#define SVG_PATH_SEGMENT_SWEEP_FLAG		(1<<1)

#define MAX_PATH_SEGMENT_POINTS 3

#define CAIRO_ENABLED 1

//-------------------------------------------------------------------------------------------------
// CSvgLoader
// Class for parsing and rendering SVG files.
// Supports a subset of SVG features.
// Uses parsifal for parsing and cairo for rendering.
//-------------------------------------------------------------------------------------------------
class CSvgLoader
{
public:
	CSvgLoader( const byte *pubSVGData, int cubSVGData ) :
		m_eCurrentElement( k_EInvalid ), 
		m_pCurrentElement( NULL ),
		m_pRootElement( NULL ),
		m_pSvgData( pubSVGData ),
		m_svgDataSize( cubSVGData ),
		m_currentDataPos( 0 ),
		m_bParseRootElementOnly( false )
	{}
	~CSvgLoader() 
	{
		if( m_pRootElement )
		{
			delete m_pRootElement;
		}
	}

	float GetWidth();
	float GetHeight();

	bool Parse();
	bool ParseRootElementOnly();

	bool Render( CUtlBuffer &bufOutput, uint32& width, uint32& height );

	void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue );

	// Parsifal callbacks
	int StartElement( const XMLCH *uri, const char *localName, const char *pchName, LPXMLVECTOR atts );
	int CDATAStart();
	int Characters( const char *Chars, int cbChars );
	int EndElement( const XMLCH *uri, const char *localName, const char *pchName );
	int cstream( BYTE *buf, int cBytes, int *cBytesActual );

	CUtlString		m_strAbortError;

private:
	enum ESvgXMLElements
	{
		// Not supported: animate, desc, filter, font, image, marker, mask, pattern, script, set, text, title, switch, view
		k_EInvalid,
		k_ERoot,
		k_ESvg,
		k_EGroup,
		k_EDefs,
		k_ESymbol,
		k_EUse,
		k_EPath,
		k_ELinearGradient,
		k_ERadialGradient,
		k_EStop,
		k_ERect,
		k_ECircle,
		k_EEllipse,
		k_ELine,
		k_EPolyline,
		k_EPolygon,
		k_EClipPath,
		k_EElementUnknown
	};
	struct RootElement;
	struct Element;

	LPXMLPARSER		m_parser;
	bool			m_bParseRootElementOnly;
	ESvgXMLElements m_eCurrentElement;
	RootElement*	m_pRootElement;
	Element			*m_pCurrentElement;
	CUtlVector< Element* > m_vecCurrentElementStack;

	CUtlSymbolTable m_elementIds;
	CUtlMap< CUtlSymbol, Element*, int, CDefLess< CUtlSymbol > > m_mapElementsById;

	const byte *m_pSvgData;
	int m_svgDataSize;
	int m_currentDataPos;

	void GetCurrentParsePosition( int *pLineOut, int *pColOut );
	int ParseError( const char *pchMsg, ... );

	enum ESvgUnit
	{
		k_EUnknown,
		k_ENumber,
		k_EPercent,
		k_EEm,
		k_EEx,
		k_EPx,
		k_ECm,
		k_EIn,
		k_EPt,
		k_EPc,
		k_EMm
	};

	enum ESvgPathSegmentType {
		k_ESegmentTypeUnknown,
		k_EClose,
		k_EMovetoAbs,
		k_EMovetoRel,
		k_ELinetoAbs,
		k_ELinetoRel,
		k_EHorizontalLinetoAbs,
		k_EHorizontalLinetoRel,
		k_EVerticalLinetoAbs,
		k_EVerticalLinetoRel,
		k_ECubicAbs,
		k_ECubicRel,
		k_ECubicSmoothAbs,
		k_ECubicSmoothRel,
		k_EQuadraticAbs,
		k_EQuadraticRel,
		k_EQuadraticSmoothAbs,
		k_EQuadraticSmoothRel,
		k_EArcAbs,
		k_EArcRel
	};

	ESvgPathSegmentType SegmentTypeFromChar( char c )
	{
		switch( c )
		{
		case 'M':
			return k_EMovetoAbs;
		case 'm':
			return k_EMovetoRel;
		case 'z':
		case 'Z':
			return k_EClose;
		case 'L':
			return k_ELinetoAbs;
		case 'l':
			return k_ELinetoRel;
		case 'H':
			return k_EHorizontalLinetoAbs;
		case 'h':
			return k_EHorizontalLinetoRel;
		case 'V':
			return k_EVerticalLinetoAbs;
		case 'v':
			return k_EVerticalLinetoRel;
		case 'C':
			return k_ECubicAbs;
		case 'c':
			return k_ECubicRel;
		case 'S':
			return k_ECubicSmoothAbs;
		case 's':
			return k_ECubicSmoothRel;
		case 'Q':
			return k_EQuadraticAbs;
		case 'q':
			return k_EQuadraticRel;
		case 'T':
			return k_EQuadraticSmoothAbs;
		case 't':
			return k_EQuadraticSmoothRel;
		case 'A':
			return k_EArcAbs;
		case 'a':
			return k_EArcRel;
		default:
			return k_ESegmentTypeUnknown;
		}
	}

	//-----------------------------------------------------------------------------
	// Presentation attributes parse, clip and render functions...
	//-----------------------------------------------------------------------------

	bool PresentationAttributeFromString( ESvgAttribute *pAttr, const char *pchName )
	{
		if( V_strcmp( pchName, "fill" ) == 0 )
		{
			*pAttr = k_ESvgAttributeFill;
		}
		else if( V_strcmp( pchName, "fill-opacity" ) == 0 )
		{
			*pAttr = k_ESvgAttributeFill_opacity;
		}
		else if( V_strcmp( pchName, "stroke" ) == 0 )
		{
			*pAttr = k_ESvgAttributeStroke;
		}
		else if( V_strcmp( pchName, "stroke-width" ) == 0 )
		{
			*pAttr = k_ESvgAttributeStroke_width;
		}
		else if( V_strcmp( pchName, "stroke-linecap" ) == 0 )
		{
			*pAttr = k_ESvgAttributeStroke_linecap;
		}
		else if( V_strcmp( pchName, "stroke-linejoin" ) == 0 )
		{
			*pAttr = k_ESvgAttributeStroke_linejoin;
		}
		else if( V_strcmp( pchName, "stroke-opacity" ) == 0 )
		{
			*pAttr = k_ESvgAttributeStroke_opacity;
		}
		else if( V_strcmp( pchName, "opacity" ) == 0 )
		{
			*pAttr = k_ESvgAttributeOpacity;
		}
		else if( V_strcmp( pchName, "fill-rule" ) == 0 )
		{
			*pAttr = k_ESvgAttributeFill_rule;
		}
		else if( V_strcmp( pchName, "clip-path" ) == 0 )
		{
			*pAttr = k_ESvgAttributeClip_path;
		}
		else if( V_strcmp( pchName, "clip-rule" ) == 0 )
		{
			*pAttr = k_ESvgAttributeClip_rule;
		}
		else
		{
			return false;
		}
		return true;
	}

	// Parse CSS-style presentation attributes, e.g. "fill-rule:evenodd;fill:#0E0F0F;"
	bool ParseCssPresentationAttributes( SvgAttributeOverrides_t *pPresentationAttributes, const char* pchStyles )
	{
		const char* pTmp = pchStyles;
		pTmp = SVGHelpers::SkipSpaces( pTmp );
		char pchToken[128];
		const char* pEnd = NULL;
		while( *pTmp )
		{
			// Read attribute name
			if( !SVGHelpers::BParseCSSToken( pchToken, V_ARRAYSIZE( pchToken ), pTmp, &pEnd ) )
			{
				return false;
			}
			pTmp = pEnd;
			pTmp = SVGHelpers::SkipSpaces( pTmp );

			if( *pTmp != ':' )
			{
				return false;
			}
			++pTmp;

			ESvgAttribute eAttrib;
			bool bValidAttrib = PresentationAttributeFromString( &eAttrib, pchToken );

			// Read attribute value
			if( !SVGHelpers::BParseCSSToken( pchToken, V_ARRAYSIZE( pchToken ), pTmp, &pEnd ) )
			{
				return false;
			}
			pTmp = pEnd;
			pTmp = SVGHelpers::SkipSpaces( pTmp );
			if( *pTmp == ';' )
			{
				++pTmp;
			}

			if( bValidAttrib )
			{
				SVGHelpers::ParsePresentationAttribute( *pPresentationAttributes, eAttrib, pchToken, &m_elementIds );
			}
		}
		return true;
	}

	bool ParsePresentationAttributes( SvgAttributeOverrides_t *pPresentationAttributes, LPXMLVECTOR atts )
	{
		if( !pPresentationAttributes )
		{
			return NULL;
		}
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			ESvgAttribute eAttrVal;
			if( PresentationAttributeFromString( &eAttrVal, pchAttrName ) )
			{
				SVGHelpers::ParsePresentationAttribute( *pPresentationAttributes, eAttrVal, pchAttrValue, &m_elementIds );
			}
			else if( V_strcmp( "style", pchAttrName ) == 0 )
			{
				// CSS style presentation attributes, e.g. style="fill-rule:evenodd;fill:#0E0F0F;"
				if( !ParseCssPresentationAttributes( pPresentationAttributes, pchAttrValue ) )
				{
					return false;
				}
			}
		}
		return true;
	}

	void StyleOverrideAttributes( SvgAttributeOverrides_t *pPresentationAttributes, ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue )
	{
		pPresentationAttributes->m_nFlags |= (1 << attributeType);
		pPresentationAttributes->m_overrides[attributeType] = *pAttributeValue;
	}

	bool GetOpacity( float& fOpacity, SvgAttributeOverrides_t *pPresentationAttributes )
	{
		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeOpacity) )
		{
			fOpacity = pPresentationAttributes->m_overrides[k_ESvgAttributeOpacity].m_opacity;
			if( fOpacity < 0.0f )
			{
				fOpacity = 0.0f;
			}
			if( fOpacity > 1.0f )
			{
				fOpacity = 1.0f;
			}
			return true;
		}
		return false;
	}

	struct ClipPathElement;

	ClipPathElement* GetClipPath( SvgAttributeOverrides_t *pPresentationAttributes )
	{
		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeClip_path) )
		{
			Element* pElement = m_mapElementsById.FindElement( pPresentationAttributes->m_overrides[k_ESvgAttributeClip_path].m_id, NULL );
			if( pElement && (pElement->m_type == k_EClipPath) )
			{
				return (ClipPathElement*)pElement;
			}
		}
		return NULL;
	}

	void ClipCurrentShape( SvgAttributeOverrides_t *pPresentationAttributes, cairo_t* pContext )
	{
#ifdef CAIRO_ENABLED
		// Clip rule
		cairo_fill_rule_t eFillRule = CAIRO_FILL_RULE_WINDING;
		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeClip_rule) )
		{
			if( pPresentationAttributes->m_overrides[k_ESvgAttributeClip_rule].m_fillRule == k_ESvgEvenodd )
			{
				eFillRule = CAIRO_FILL_RULE_EVEN_ODD;
			}
		}
		cairo_set_fill_rule( pContext, eFillRule );

		cairo_clip( pContext );
#endif
	}

	void RenderCurrentShape( SvgAttributeOverrides_t *pPresentationAttributes, cairo_t* pContext )
	{
#ifdef CAIRO_ENABLED
		// Fill rule
		cairo_fill_rule_t eFillRule = CAIRO_FILL_RULE_WINDING;
		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeFill_rule) )
		{
			if( pPresentationAttributes->m_overrides[k_ESvgAttributeFill_rule].m_fillRule == k_ESvgEvenodd )
			{
				eFillRule = CAIRO_FILL_RULE_EVEN_ODD;
			}
		}
		cairo_set_fill_rule( pContext, eFillRule );

		// Fill
		float r = 0.0f, g = 0.0f , b = 0.0f; // default to black
		float a = 1.0f; //default to fully opaque
		bool bFillNone = false;
		GradientElement* pFillGradient = NULL;

		// stroke
		// default stroke="none"
		float sr = 0.0f, sg = 0.0f, sb = 0.0f;
		float sa = 1.0f; //default to fully opaque
		bool bStrokeNone = true;
		GradientElement* pStrokeGradient = NULL;

		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeFill) )
		{
			if( pPresentationAttributes->m_nFlags & SVG_FILL_ATTRIBUTE_IS_URL )
			{
				Element* pElement = m_mapElementsById.FindElement( pPresentationAttributes->m_overrides[k_ESvgAttributeFill].m_id, NULL );
				if( pElement && ((pElement->m_type == k_ELinearGradient) || (pElement->m_type == k_ERadialGradient)) )
				{
					pFillGradient = (GradientElement*)pElement;
				}
			}
			else
			{
				unsigned char* color = pPresentationAttributes->m_overrides[k_ESvgAttributeFill].m_color;
				r = (float)color[0] / 255.0f;
				g = (float)color[1] / 255.0f;
				b = (float)color[2] / 255.0f;
				bFillNone = (color[3] == 0);
			}
		}

		if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeStroke) )
		{
			if( pPresentationAttributes->m_nFlags & SVG_STROKE_ATTRIBUTE_IS_URL )
			{
				Element* pElement = m_mapElementsById.FindElement( pPresentationAttributes->m_overrides[k_ESvgAttributeStroke].m_id, NULL );
				if( pElement && ((pElement->m_type == k_ELinearGradient) || (pElement->m_type == k_ERadialGradient)) )
				{
					pStrokeGradient = (GradientElement*)pElement;
					bStrokeNone = false;
				}
			}
			else
			{
				unsigned char* color = pPresentationAttributes->m_overrides[k_ESvgAttributeStroke].m_color;
				sr = (float)color[0] / 255.0f;
				sg = (float)color[1] / 255.0f;
				sb = (float)color[2] / 255.0f;
				bStrokeNone = (color[3] == 0);
			}
		}

		if( !bFillNone )
		{
			if( pFillGradient )
			{
				ApplyGradient( pFillGradient, pContext );
			}
			else
			{
				if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeFill_opacity) )
				{
					a = pPresentationAttributes->m_overrides[k_ESvgAttributeFill_opacity].m_opacity;
				}
				cairo_set_source_rgba( pContext, b, g, r, a ); //Swap red and blue
			}

			if( !bStrokeNone )
			{
				// cairo_fill_preserve preserves the current path within the cairo context
				cairo_fill_preserve( pContext );
			}
			else
			{
				// cairo_fill clears the current path from the cairo context
				cairo_fill( pContext );
			}
		}

		if( !bStrokeNone )
		{
			float fStrokeWidth = 1.0f;
			if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeStroke_width) )
			{
				fStrokeWidth = pPresentationAttributes->m_overrides[k_ESvgAttributeStroke_width].m_length;
			}
			cairo_set_line_width( pContext, fStrokeWidth );

			cairo_line_cap_t eLineCap = CAIRO_LINE_CAP_BUTT;
			if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeStroke_linecap) )
			{
				switch( pPresentationAttributes->m_overrides[k_ESvgAttributeStroke_linecap].m_strokeLineCap )
				{
				case k_ESvgButt:
					eLineCap = CAIRO_LINE_CAP_BUTT;
					break;
				case k_ESvgCapRound:
					eLineCap = CAIRO_LINE_CAP_ROUND;
					break;
				case k_ESvgSquare:
					eLineCap = CAIRO_LINE_CAP_SQUARE;
					break;
				default:
					break;
				}
			}
			cairo_set_line_cap( pContext, eLineCap );

			cairo_line_join_t eLineJoin = CAIRO_LINE_JOIN_MITER;
			if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeStroke_linejoin) )
			{
				switch( pPresentationAttributes->m_overrides[k_ESvgAttributeStroke_linejoin].m_strokeLineJoin )
				{
				case k_ESvgMiter:
					eLineJoin = CAIRO_LINE_JOIN_MITER;
					break;
				case k_ESvgJoinRound:
					eLineJoin = CAIRO_LINE_JOIN_ROUND;
					break;
				case k_ESvgBevel:
					eLineJoin = CAIRO_LINE_JOIN_BEVEL;
					break;
				default:
					break;
				}
			}
			cairo_set_line_join( pContext, eLineJoin );

			if( pStrokeGradient )
			{
				ApplyGradient( pStrokeGradient, pContext );
			}
			else
			{
				if( pPresentationAttributes->m_nFlags & (1 << k_ESvgAttributeStroke_opacity) )
				{
					sa = pPresentationAttributes->m_overrides[k_ESvgAttributeStroke_opacity].m_opacity;
				}
				cairo_set_source_rgba( pContext, sb, sg, sr, sa ); //Swap red and blue
			}

			cairo_stroke( pContext );
		}
#endif
	}

	const char* GetAttVal( LPXMLVECTOR atts, const char* attName )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			if( V_strcmp( pchAttrName, attName ) == 0 )
			{
				return pchAttrValue;
			}
		}
		return NULL;
	}

	//-----------------------------------------------------------------------------
	// SVG Transforms: parse and apply functions...
	//-----------------------------------------------------------------------------

	enum ETransformType {
		k_EMatrix,
		k_ETranslate,
		k_EScale,
		k_ERotate,
		k_ESkewX,
		k_ESkewY
	};

	struct Transform
	{
		Transform() : m_eTransformType( k_EMatrix ) { for( int i = 0; i < 6; ++i ) { m_fVals[i] = 0.0f; } }
		ETransformType	m_eTransformType;
		float m_fVals[6];
	};

	bool ParseTransforms( CUtlVector<Transform> *pVecTransforms, const char* pVal )
	{
		const char* pTmp = pVal;
		pTmp = SVGHelpers::SkipSpaces( pTmp );

		while( *pTmp )
		{
			int iTransform = pVecTransforms->AddToTail();
			Transform& t = pVecTransforms->Element( iTransform );
			if( V_strncmp( pTmp, "matrix", V_strlen( "matrix" ) ) == 0 )
			{
				pTmp += V_strlen( "matrix" );
				t.m_eTransformType = k_EMatrix;
			}
			else if( V_strncmp( pTmp, "translate", V_strlen( "translate" ) ) == 0 )
			{
				pTmp += V_strlen( "translate" );
				t.m_eTransformType = k_ETranslate;
			}
			else if( V_strncmp( pTmp, "scale", V_strlen( "scale" ) ) == 0 )
			{
				pTmp += V_strlen( "scale" );
				t.m_eTransformType = k_EScale;
			}
			else if( V_strncmp( pTmp, "rotate", V_strlen( "rotate" ) ) == 0 )
			{
				pTmp += V_strlen( "rotate" );
				t.m_eTransformType = k_ERotate;
			}
			else if( V_strncmp( pTmp, "skewX", V_strlen( "skewX" ) ) == 0 )
			{
				pTmp += V_strlen( "skewX" );
				t.m_eTransformType = k_ESkewX;
			}
			else if( V_strncmp( pTmp, "skewY", V_strlen( "skewY" ) ) == 0 )
			{
				pTmp += V_strlen( "skewY" );
				t.m_eTransformType = k_ESkewY;
			}
			else
			{
				ParseError( "Unknown transform type %s", pTmp );
				return false;
			}

			if( *pTmp != '(' )
			{
				ParseError( "Missing open bracket in transform %s", pVal );
				return false;
			}
			++pTmp;
			pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );

			const char* pEnd = NULL;
			switch( t.m_eTransformType )
			{
			case k_EMatrix:
				for( int i = 0; i < 6; ++i )
				{
					if( !SVGHelpers::BParseFloat( &t.m_fVals[i], pTmp, &pEnd ) )
					{
						ParseError( "Error parsing transform - invalid matrix - %s", pVal );
						return false;
					}
					pTmp = pEnd;
					pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
				}
				break;
			case k_ETranslate:
			case k_EScale:
				if( !SVGHelpers::BParseFloat( &t.m_fVals[0], pTmp, &pEnd ) )
				{
					ParseError( "Error parsing transform - invalid translate - %s", pVal );
					return false;
				}
				pTmp = pEnd;
				pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
				if( *pTmp == ')' ) // y-value is optional
				{
					// No y-value supplied
					if( t.m_eTransformType == k_EScale )
					{
						t.m_fVals[1] = t.m_fVals[0]; // Default scaley to scalex
					}
					else
					{
						t.m_fVals[1] = 0.0f;
					}
				}
				else
				{
					// Parse y-value
					if( !SVGHelpers::BParseFloat( &t.m_fVals[1], pTmp, &pEnd ) )
					{
						ParseError( "Error parsing transform - invalid translate - %s", pVal );
						return false;
					}
					pTmp = pEnd;
					pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
				}
				break;
			case k_ERotate:
				if( !SVGHelpers::BParseFloat( &t.m_fVals[0], pTmp, &pEnd ) )
				{
					ParseError( "Error parsing transform - invalid rotation angle - %s", pVal );
					return false;
				}
				pTmp = pEnd;
				pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
				if( *pTmp == ')' ) // centre point is optional
				{
					t.m_fVals[1] = 0.0f;
					t.m_fVals[2] = 0.0f;
				}
				else
				{
					// Parse centre-point
					for( int i = 1; i < 3; ++i )
					{
						if( !SVGHelpers::BParseFloat( &t.m_fVals[i], pTmp, &pEnd ) )
						{
							ParseError( "Error parsing transform - invalid rotation centre point - %s", pVal );
							return false;
						}
						pTmp = pEnd;
						pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
					}
				}
				break;
			case k_ESkewX:
			case k_ESkewY:
				if( !SVGHelpers::BParseFloat( &t.m_fVals[0], pTmp, &pEnd ) )
				{
					ParseError( "Error parsing transform - invalid skew angle - %s", pVal );
					return false;
				}
				pTmp = pEnd;
				pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
				break;
			default:
				ParseError( "Unexpected transform type");
				return false;
			}

			if( *pTmp != ')' )
			{
				ParseError( "Missing close bracket in transform %s", pVal );
				return false;
			}
			++pTmp;
			pTmp = SVGHelpers::SkipSpacesOrComma( pTmp );
		}
		return true;
	}

	bool ParseTransforms( CUtlVector<Transform> *pVecTransforms, LPXMLVECTOR atts )
	{
		if( !pVecTransforms )
		{
			return false;
		}
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			if( V_strcmp( "transform", pchAttrName ) == 0 )
			{
				return ParseTransforms( pVecTransforms, pchAttrValue );
			}
		}

		return true;
	}

	struct TransformState
	{
		TransformState() : m_bTransformsApplied( false ) {}
		bool m_bTransformsApplied;
		cairo_matrix_t m_previousMatrix;
	};

	// Apply the list of transforms (pTransforms)
	// Store the previous cairo transform matrix in the supplied TransformState (tState) so it can be restored later.  
	static void SaveAndApplyTransforms( CUtlVector< Transform> *pTransforms, TransformState& tState, cairo_t *pContext )
	{
#ifdef CAIRO_ENABLED
		tState.m_bTransformsApplied = false;
		if( pTransforms->Count() > 0 )
		{
			cairo_get_matrix( pContext, &tState.m_previousMatrix );
			FOR_EACH_VEC( *pTransforms, i )
			{
				Transform& t = pTransforms->Element( i );
				cairo_matrix_t m;
				switch( t.m_eTransformType )
				{
				case k_EMatrix:
					cairo_matrix_init( &m, t.m_fVals[0], t.m_fVals[1], t.m_fVals[2], t.m_fVals[3], t.m_fVals[4], t.m_fVals[5] );
					cairo_transform( pContext, &m );
					break;
				case k_ETranslate:
					cairo_translate( pContext, t.m_fVals[0], t.m_fVals[1] );
					break;
				case k_EScale:
					cairo_scale( pContext, t.m_fVals[0], t.m_fVals[1] );
					break;
				case k_ERotate:
					cairo_translate( pContext, t.m_fVals[1], t.m_fVals[2] );
					cairo_rotate( pContext, DEG2RAD( t.m_fVals[0] ) );
					cairo_translate( pContext, -t.m_fVals[1], -t.m_fVals[2] );
					break;
				case k_ESkewX:
					cairo_matrix_init( &m, 1.0, 0.0, tan( DEG2RAD( t.m_fVals[0] ) ), 1.0, 0.0, 0.0 );
					cairo_transform( pContext, &m );
					break;
				case k_ESkewY:
					cairo_matrix_init( &m, 1.0, tan( DEG2RAD( t.m_fVals[0] ) ), 0.0, 1.0, 0.0, 0.0 );
					cairo_transform( pContext, &m );
					break;
				default:
					Msg( "Unknown transform type\n" );
					break;
				}
			}

			tState.m_bTransformsApplied = true;

		}
#endif
	}

	static void RestoreTransforms( TransformState& tState, cairo_t *pContext )
	{
#ifdef CAIRO_ENABLED
		if( tState.m_bTransformsApplied )
		{
			cairo_set_matrix( pContext, &tState.m_previousMatrix );
		}
#endif
	}

	//----------------------------------------------------------------------------
	// SvgLength
	// In theory SVG lengths can be supplied in various units, including percentages.
	// For now we assume all values are supplied in pixels.
	//----------------------------------------------------------------------------
	struct SvgLength
	{
		SvgLength() : m_val( 0.0f ), m_unit( k_EPx ) {}
		SvgLength( float val, ESvgUnit unit ) : m_val( val ), m_unit( unit ) {}

		float m_val;
		ESvgUnit m_unit;
	};

	bool ParseLength( SvgLength *pLength, const char* pchValue )
	{
		int strLen = V_strlen( pchValue );
		if( strLen <= 0 )
		{
			return false;
		}

		pLength->m_val = V_atof( pchValue );

		const char* pUnit = &pchValue[strLen - 1];
		while( V_isspace( *pUnit ) && (pUnit > pchValue) )
		{
			--pUnit;
		}

		pLength->m_unit = k_ENumber;
		if( (*pUnit == '%') )
		{
			pLength->m_unit = k_EPercent;
		}
		else if( pUnit > pchValue ) // Check not already at beginning of string
		{
			--pUnit;
			if( V_strnicmp( pUnit, "em", 2 ) == 0 )
			{
				pLength->m_unit = k_EEm;
			}
			else if( V_strnicmp( pUnit, "ex", 2 ) == 0 )
			{
				pLength->m_unit = k_EEx;
			}
			else if( V_strnicmp( pUnit, "px", 2 ) == 0 )
			{
				pLength->m_unit = k_EPx;
			}
			else if( V_strnicmp( pUnit, "cm", 2 ) == 0 )
			{
				pLength->m_unit = k_ECm;
			}
			else if( V_strnicmp( pUnit, "mm", 2 ) == 0 )
			{
				pLength->m_unit = k_EMm;
			}
			else if( V_strnicmp( pUnit, "in", 2 ) == 0 )
			{
				pLength->m_unit = k_EIn;
			}
			else if( V_strnicmp( pUnit, "pt", 2 ) == 0 )
			{
				pLength->m_unit = k_EPt;
			}
			else if( V_strnicmp( pUnit, "pc", 2 ) == 0 )
			{
				pLength->m_unit = k_EPc;
			}
			else
			{
				pLength->m_unit = k_ENumber;
			}
		}
		return true;
	}

	int ElementTypeFromString( const char *pchName )
	{
		ESvgXMLElements elementType = k_EElementUnknown;
		if( V_strcmp( pchName, "svg" ) == 0 )
		{
			elementType = k_ESvg;
		}
		else if( V_strcmp( pchName, "g" ) == 0 )
		{
			elementType = k_EGroup;
		}
		else if( V_strcmp( pchName, "defs" ) == 0 )
		{
			elementType = k_EDefs;
		}
		else if( V_strcmp( pchName, "symbol" ) == 0 )
		{
			elementType = k_ESymbol;
		}
		else if( V_strcmp( pchName, "use" ) == 0 )
		{
			elementType = k_EUse;
		}
		else if( V_strcmp( pchName, "path" ) == 0 )
		{
			elementType = k_EPath;
		}
		else if( V_strcmp( pchName, "linearGradient" ) == 0 )
		{
			elementType = k_ELinearGradient;
		}
		else if( V_strcmp( pchName, "radialGradient" ) == 0 )
		{
			elementType = k_ERadialGradient;
		}
		else if( V_strcmp( pchName, "stop" ) == 0 )
		{
			elementType = k_EStop;
		}
		else if( V_strcmp( pchName, "rect" ) == 0 )
		{
			elementType = k_ERect;
		}
		else if( V_strcmp( pchName, "circle" ) == 0 )
		{
			elementType = k_ECircle;
		}
		else if( V_strcmp( pchName, "ellipse" ) == 0 )
		{
			elementType = k_EEllipse;
		}
		else if( V_strcmp( pchName, "line" ) == 0 )
		{
			elementType = k_ELine;
		}
		else if( V_strcmp( pchName, "polyline" ) == 0 )
		{
			elementType = k_EPolyline;
		}
		else if( V_strcmp( pchName, "polygon" ) == 0 )
		{
			elementType = k_EPolygon;
		}
		else if( V_strcmp( pchName, "clipPath" ) == 0 )
		{
			elementType = k_EClipPath;
		}
		return elementType;
	}

	struct ContainerElement;

	//----------------------------------------------------------------------------
	// Element
	// Base class for all SVG elements
	//----------------------------------------------------------------------------
	struct Element
	{
		Element( ESvgXMLElements elementType, CSvgLoader* pSvg ) : m_pParent( NULL ), m_pSvg( pSvg ), m_type( elementType ), m_id( UTL_INVAL_SYMBOL ) {}
		virtual ~Element() {}

		ContainerElement* m_pParent;
		CSvgLoader* m_pSvg;
		CUtlSymbol m_id;
		ESvgXMLElements m_type;

		virtual void Render( cairo_t* pContext ) {}
		virtual void Clip( cairo_t* pContext ) {}
		virtual void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue ) {}
	};

	bool ParseElement( Element *pElement, LPXMLVECTOR atts )
	{
		const char* pId = GetAttVal( atts, "id" );
		if( pId )
		{
			pElement->m_id = m_elementIds.AddString( pId ); // Find or add
			m_mapElementsById.InsertOrReplace( pElement->m_id, pElement );
		}
		return true;
	}

	//----------------------------------------------------------------------------
	// ContainerElement
	// Base class for all SVG container elements
	//----------------------------------------------------------------------------
	struct ContainerElement : public Element
	{
		ContainerElement( ESvgXMLElements type, CSvgLoader* pSvg ) : Element( type, pSvg ) {}
		~ContainerElement()
		{
			FOR_EACH_VEC( m_vecChildren, i )
			{
				delete m_vecChildren[i];
			}
		}

		virtual void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue )
		{
			m_pSvg->StyleOverrideContainer( this, attributeType, pAttributeValue );
		}

		SvgAttributeOverrides_t m_presentationAttributes;
		CUtlVector< Element* > m_vecChildren;
	};

	bool ParseContainer( ContainerElement *pContainer, LPXMLVECTOR atts )
	{
		ParseElement( pContainer, atts );
		ParsePresentationAttributes( &pContainer->m_presentationAttributes, atts );
		return true;
	}

	void RenderContainer( ContainerElement *pContainer, cairo_t* pContext )
	{
#ifdef CAIRO_ENABLED
		float fOpacity;
		bool bUseOpacityGroup = GetOpacity( fOpacity, &pContainer->m_presentationAttributes ) && (fOpacity < 1.0f);

		if( bUseOpacityGroup )
		{
			cairo_push_group( pContext );
		}

		ClipPathElement* pClipPath = GetClipPath( &pContainer->m_presentationAttributes );
		if( pClipPath )
		{
			cairo_save( pContext );
			FOR_EACH_VEC( pClipPath->m_vecChildren, i )
			{
				pClipPath->m_vecChildren[i]->Clip( pContext );
			}
		}

		FOR_EACH_VEC( pContainer->m_vecChildren, i )
		{
			pContainer->m_vecChildren[i]->Render( pContext );
		}

		if( pClipPath )
		{
			cairo_restore( pContext );
		}

		if( bUseOpacityGroup )
		{
			cairo_pop_group_to_source( pContext );
			cairo_paint_with_alpha( pContext, fOpacity );
		}
#endif
	}

	void StyleOverrideContainer( ContainerElement *pContainer, ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue )
	{
		StyleOverrideAttributes( &pContainer->m_presentationAttributes, attributeType, pAttributeValue );

		FOR_EACH_VEC( pContainer->m_vecChildren, i )
		{
			pContainer->m_vecChildren[i]->StyleOverride( attributeType, pAttributeValue );
		}
	}

	bool AddChildElement( Element* pElement )
	{
		if( !m_vecCurrentElementStack.Count() )
		{
			Msg( "Error: Attempting to add child element before parsing SVG root\n" );
			return false;
		}

		ContainerElement *pParent = dynamic_cast<ContainerElement *>(m_vecCurrentElementStack.Tail());
		if( !pParent )
		{
			Msg( "Error: Attempting to add child element to a non-container element\n" );
			return false;
		}
		pParent->m_vecChildren.AddToTail( pElement );
		pElement->m_pParent = pParent;

		// this panel is now top of the stack
		m_vecCurrentElementStack.AddToTail( pElement );
		return true;
	}

	//----------------------------------------------------------------------------
	// RootElement
	// The SVG root element <svg>
	//----------------------------------------------------------------------------

	struct RootElement : public ContainerElement
	{
		RootElement( CSvgLoader* pSvg ) : ContainerElement( k_ESvg, pSvg )
		{
			for( int i = 0; i < 4; ++i )
			{
				m_viewBox[i] = 0.0f;
			}
		}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderRootElement( this, pContext ); }

		SvgLength m_x;
		SvgLength m_y;
		SvgLength m_width;
		SvgLength m_height;
		float	  m_viewBox[4];
	};

	bool ParseRootElement( RootElement *pRootElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			if( V_strcmp( "x", pchAttrName ) == 0 )
			{
				ParseLength( &pRootElement->m_x, pchAttrValue );
			}
			else if( V_strcmp( "y", pchAttrName ) == 0 )
			{
				ParseLength( &pRootElement->m_y, pchAttrValue );
			}
			else if( V_strcmp( "width", pchAttrName ) == 0 )
			{
				ParseLength( &pRootElement->m_width, pchAttrValue );
			}
			else if( V_strcmp( "height", pchAttrName ) == 0 )
			{
				ParseLength( &pRootElement->m_height, pchAttrValue );
			}
			else if( V_strcmp( "viewBox", pchAttrName ) == 0 )
			{
				const char* pVal = pchAttrValue;
				const char* pEnd = NULL;
				for( int j = 0; j < 4; ++j )
				{
					pVal = SVGHelpers::SkipSpacesOrComma( pVal );
					if( !SVGHelpers::BParseFloat( &pRootElement->m_viewBox[j], pVal, &pEnd ) )
					{
						return false;
					}
					pVal = pEnd;
				}
			}
		}
		ParseContainer( pRootElement, atts );
		m_vecCurrentElementStack.AddToTail( pRootElement );
		return true;
	}

	void RenderRootElement( RootElement *pRootElement, cairo_t* pContext )
	{
#ifdef CAIRO_ENABLED
		float* pViewBox = pRootElement->m_viewBox;

		// Apply viewbox transform if viewbox rectangle has non-zero width and height, and they differ from the file width and height.
		if( pViewBox[2] > 0.0f && pViewBox[3] > 0.0f )
		{
			if( (pRootElement->m_width.m_val != pViewBox[2]) || (pRootElement->m_height.m_val != pViewBox[3]) )
			{
				cairo_scale( pContext,
					(double)pRootElement->m_width.m_val / (double)pViewBox[2],
					(double)pRootElement->m_height.m_val / (double)pViewBox[3] );
			}
		}

		if( pViewBox[0] != 0.0f && pViewBox[1] != 0.0f )
		{
			cairo_translate( pContext, pViewBox[0], pViewBox[1] );
		}

		RenderContainer( pRootElement, pContext );
#endif
	}

	//----------------------------------------------------------------------------
	// GroupElement <g>
	//----------------------------------------------------------------------------

	struct GroupElement : public ContainerElement
	{
		GroupElement( CSvgLoader *pSvg ) : ContainerElement( k_EGroup, pSvg ) {}
		GroupElement( ESvgXMLElements type, CSvgLoader *pSvg ) : ContainerElement( type, pSvg ) {}
		~GroupElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderGroupElement( this, pContext ); }

		CUtlVector<Transform> m_vecTransforms;
	};

	bool ParseGroupElement( GroupElement* pGroup, LPXMLVECTOR atts )
	{
		ParseContainer( pGroup, atts );
		ParseTransforms( &pGroup->m_vecTransforms, atts );

		return AddChildElement( pGroup );
	}

	void RenderGroupElement( GroupElement *pGroupElement, cairo_t* pContext )
	{
		TransformState tState;
		SaveAndApplyTransforms( &pGroupElement->m_vecTransforms, tState, pContext );

		RenderContainer( pGroupElement, pContext );

		RestoreTransforms( tState, pContext );
	}


	//----------------------------------------------------------------------------
	// DefsElement <defs>
	// Defs elements are similar to group elements, but are not rendered directly
	// They contain elements that can be referred to by use elements
	//----------------------------------------------------------------------------
	struct DefsElement : public GroupElement
	{
		DefsElement( CSvgLoader *pSvg ) : GroupElement( k_EDefs, pSvg) {}
		~DefsElement() {}

		virtual void Render( cairo_t* pContext ) {} // Defs elements are not rendered directly
	};

	bool ParseDefsElement( DefsElement* pDefs, LPXMLVECTOR atts )
	{
		return ParseGroupElement( pDefs, atts );
	}

	//----------------------------------------------------------------------------
	// SymbolElement <symbol>
	// Symbol elements are similar to group elements, but are not rendered directly
	// They can be referred to by use elements
	// Symbol elements don't support the "transform" attribute
	//----------------------------------------------------------------------------
	struct SymbolElement : public ContainerElement
	{
		SymbolElement( CSvgLoader *pSvg ) : ContainerElement( k_ESymbol, pSvg ) {}
		~SymbolElement() {}

		virtual void Render( cairo_t* pContext ) {}  // Symbol elements are not rendered directly
	};

	bool ParseSymbolElement( SymbolElement* pSymbol, LPXMLVECTOR atts )
	{
		ParseContainer( pSymbol, atts );

		return AddChildElement( pSymbol );
	}

	//----------------------------------------------------------------------------
	// ClipPathElement <clipPath>
	//----------------------------------------------------------------------------
	struct ClipPathElement : public ContainerElement
	{
		ClipPathElement( CSvgLoader *pSvg ) : ContainerElement( k_EClipPath, pSvg ) {}
		~ClipPathElement() {}

		virtual void Render( cairo_t* pContext ) {}  // Clip path elements are not rendered directly
		virtual void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue ) {} // Not applicable for clip paths
	};

	bool ParseClipPathElement( ClipPathElement* pClipPath, LPXMLVECTOR atts )
	{
		ParseContainer( pClipPath, atts );

		return AddChildElement( pClipPath );
	}

	//----------------------------------------------------------------------------
	// GraphicsElement
	// Base class for elements that can cause graphics to be drawn onto the target canvas.
	// Specifically: circle, ellipse, line, path, polygon, polyline, rect, and use. 
	// (n.b. 'image' and 'text' are also graphics elements as per SVG spec, but not supported in this implementation)
	//----------------------------------------------------------------------------
	struct GraphicsElement : public Element
	{
		GraphicsElement( ESvgXMLElements type, CSvgLoader *pSvg ) : Element( type, pSvg ) {}
		~GraphicsElement() {}

		virtual void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue )
		{
			m_pSvg->StyleOverrideAttributes( &m_presentationAttributes, attributeType, pAttributeValue );
		}

		SvgAttributeOverrides_t m_presentationAttributes;
		CUtlVector<Transform> m_vecTransforms;
	};

	bool ParseGraphicsElement( GraphicsElement *pGraphicsElement, LPXMLVECTOR atts )
	{
		if( !ParseElement( pGraphicsElement, atts ) )
		{
			return false;
		}
		if( !ParseTransforms( &pGraphicsElement->m_vecTransforms, atts ) )
		{
			return false;
		}
		if( !ParsePresentationAttributes( &pGraphicsElement->m_presentationAttributes, atts ) )
		{
			return false;
		}
		return AddChildElement( pGraphicsElement );
	}

	struct GraphicsElementRenderState
	{
		GraphicsElementRenderState( cairo_t* pContext ) : m_bUseOpacityGroup( false ), m_bUseClipPath( false ), m_pContext( pContext ) {}
		TransformState m_transformState;
		bool m_bUseOpacityGroup;
		float m_fOpacity;
		bool m_bUseClipPath;
		cairo_t* m_pContext;
	};

	void PreRenderGraphicsElement( GraphicsElement *pGraphicsElement, GraphicsElementRenderState &state, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		SaveAndApplyTransforms( &pGraphicsElement->m_vecTransforms, state.m_transformState, state.m_pContext );

		if( !bRenderForClip )
		{
			state.m_bUseOpacityGroup = GetOpacity( state.m_fOpacity, &pGraphicsElement->m_presentationAttributes ) && (state.m_fOpacity < 1.0f);

			if( state.m_bUseOpacityGroup )
			{
				cairo_push_group( state.m_pContext );
			}

			ClipPathElement* pClipPath = GetClipPath( &pGraphicsElement->m_presentationAttributes );
			if( pClipPath )
			{
				state.m_bUseClipPath = true;

				cairo_save( state.m_pContext );
				FOR_EACH_VEC( pClipPath->m_vecChildren, i )
				{
					pClipPath->m_vecChildren[i]->Clip( state.m_pContext );
				}
			}
			else
			{
				state.m_bUseClipPath = false;
			}

		}
#endif
	}

	void PostRenderGraphicsElement( GraphicsElementRenderState &state, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		if( !bRenderForClip )
		{
			if( state.m_bUseClipPath )
			{
				cairo_restore( state.m_pContext );
			}

			if( state.m_bUseOpacityGroup )
			{
				cairo_pop_group_to_source( state.m_pContext );
				cairo_paint_with_alpha( state.m_pContext, state.m_fOpacity );
			}
		}

		RestoreTransforms( state.m_transformState, state.m_pContext );
#endif
	}

	//----------------------------------------------------------------------------
	// UseElement <use>
	//----------------------------------------------------------------------------
	struct UseElement : public GraphicsElement
	{
		UseElement( CSvgLoader *pSvg ) : GraphicsElement( k_EUse, pSvg ), m_x( 0.0f ), m_y( 0.0f ), m_width( 0.0f ), m_height( 0.0f ), m_refId( UTL_INVAL_SYMBOL ) {}
		~UseElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderUseElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderUseElement( this, pContext, true ); }

		float m_x;
		float m_y;
		float m_width;
		float m_height;
		CUtlSymbol m_refId;
	};

	bool ParseUseElement( UseElement* pUseElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;
			if( V_strcmp( "x", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pUseElement->m_x, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pUseElement->m_y, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "width", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pUseElement->m_width, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "height", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pUseElement->m_height, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "xlink:href", pchAttrName ) == 0 )
			{
				const char* pTmp = pchAttrValue;
				pTmp = SVGHelpers::SkipSpaces( pTmp );
				if( *pTmp != '#' )
				{
					return false;
				}
				++pTmp;
				pUseElement->m_refId = m_elementIds.AddString( pTmp ); // Find or add
			}
		}

		if( !ParseGraphicsElement( pUseElement, atts ) )
		{
			return false;
		}

		if( (pUseElement->m_x != 0.0f) || (pUseElement->m_y != 0.0f) )
		{
			int iTransform = pUseElement->m_vecTransforms.AddToTail();
			Transform& t = pUseElement->m_vecTransforms.Element( iTransform );
			t.m_eTransformType = k_ETranslate;
			t.m_fVals[0] = pUseElement->m_x;
			t.m_fVals[1] = pUseElement->m_y;
		}

		return true;
	}

	void RenderUseElement( UseElement *pUseElement, cairo_t* pContext, bool bRenderForClip = false )
	{
		Element* pRefElement = m_mapElementsById.FindElement( pUseElement->m_refId, NULL );
		if( pRefElement )
		{
			GraphicsElementRenderState renderState( pContext );
			PreRenderGraphicsElement( pUseElement, renderState, bRenderForClip );

			if( bRenderForClip )
			{
				pRefElement->Clip( pContext );
			}
			else
			{
				pRefElement->Render( pContext );
			}

			PostRenderGraphicsElement( renderState, bRenderForClip );
		}
	}

	//----------------------------------------------------------------------------
	// PathElement <path>
	//----------------------------------------------------------------------------

	struct PathSegment
	{
		PathSegment() : m_type( k_ESegmentTypeUnknown ), m_flags( 0 ) {
			for( int i = 0; i < MAX_PATH_SEGMENT_POINTS; ++i ) { m_points[i].Init( 0.0f, 0.0f ); }
		}
		ESvgPathSegmentType m_type;
		Vector2D m_points[MAX_PATH_SEGMENT_POINTS];
		uint32 m_flags;
	};

	struct PathElement : public GraphicsElement
	{
		PathElement( CSvgLoader *pSvg ) : GraphicsElement( k_EPath, pSvg ) {}
		~PathElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderPathElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderPathElement( this, pContext, true ); }

		CUtlVector<PathSegment> m_vecSegments;
		// Path length not supported
	};

	bool ParsePath( CUtlVector<PathSegment> *pVecSegments, const char* pchAttrValue )
	{
		const char* pVal = pchAttrValue;
		int tempInt = 0;

		pVal = SVGHelpers::SkipSpaces( pVal );

		ESvgPathSegmentType prevSegType = k_ESegmentTypeUnknown;
		while( *pVal )
		{
			int iSeg = pVecSegments->AddToTail();
			PathSegment& segment = pVecSegments->Element(iSeg);

			ESvgPathSegmentType segType = SegmentTypeFromChar( *pVal );
			if( segType != k_ESegmentTypeUnknown )
			{
				segment.m_type = segType;
				++pVal;
			}
			else if( prevSegType == k_ESegmentTypeUnknown )
			{
				segment.m_type = k_EMovetoAbs;
			}
			else if( prevSegType == k_EMovetoAbs )
			{
				segment.m_type = k_ELinetoAbs;
			}
			else if( prevSegType == k_EMovetoRel )
			{
				segment.m_type = k_ELinetoRel;
			}
			else
			{
				segment.m_type = prevSegType;
			}

			prevSegType = segment.m_type;

			pVal = SVGHelpers::SkipSpaces( pVal );

			const char* pEnd;
			switch( segment.m_type )
			{
			case k_EMovetoAbs:
			case k_EMovetoRel:
			case k_ELinetoAbs:
			case k_ELinetoRel:
			case k_EQuadraticSmoothAbs:
			case k_EQuadraticSmoothRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_EClose:
				break;
			case k_EHorizontalLinetoAbs:
			case k_EHorizontalLinetoRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_EVerticalLinetoAbs:
			case k_EVerticalLinetoRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_ECubicAbs:
			case k_ECubicRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_ECubicSmoothAbs:
			case k_ECubicSmoothRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_EQuadraticAbs:
			case k_EQuadraticRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			case k_EArcAbs:
			case k_EArcRel:
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[0].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[1].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseInt( &tempInt, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				if( tempInt != 0 )
				{
					segment.m_flags |= SVG_PATH_SEGMENT_LARGE_ARC_FLAG;
				}
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseInt( &tempInt, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				if( tempInt != 0 )
				{
					segment.m_flags |= SVG_PATH_SEGMENT_SWEEP_FLAG;
				}
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].x, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				pVal = SVGHelpers::SkipSpacesOrComma( pVal );
				if( !SVGHelpers::BParseFloat( &segment.m_points[2].y, pVal, &pEnd ) )
				{
					return false;
				}
				pVal = pEnd;
				break;
			default:
				break;
			}
			pVal = SVGHelpers::SkipSpacesOrComma( pVal );
		}
		
		return true;
	}

	bool ParsePathElement( PathElement *pPathElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			if( V_strcmp( "d", pchAttrName ) == 0 )
			{
				if( !ParsePath( &pPathElement->m_vecSegments, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pPathElement, atts );
	}

	void RenderPathElement( PathElement *pPathElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		if( pPathElement->m_vecSegments.Count() == 0 )
		{
			return;
		}

		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pPathElement, renderState, bRenderForClip );

		cairo_new_path( pContext );

		PathSegment* pPrevSegment = NULL;
		FOR_EACH_VEC( pPathElement->m_vecSegments, i )
		{
			PathSegment* pSegment = &pPathElement->m_vecSegments[i];
			Vector2D* pPoints = pSegment->m_points;

			if( (i == 0) &&
				((pSegment->m_type == k_EMovetoRel) || (pSegment->m_type == k_ELinetoRel) ||
				(pSegment->m_type == k_EVerticalLinetoRel) || (pSegment->m_type == k_EHorizontalLinetoRel) ||
				(pSegment->m_type == k_ECubicRel) || (pSegment->m_type == k_ECubicSmoothRel) ||
				(pSegment->m_type == k_EQuadraticRel) || (pSegment->m_type == k_EQuadraticSmoothRel)) )
			{
				// Relative cairo path commands will fail if there's no current point, so set to (0,0) 
				if( !cairo_has_current_point( pContext ) )
				{
					cairo_move_to( pContext, 0.0f, 0.0f );
				}
			}

			switch( pSegment->m_type )
			{
			case k_EMovetoAbs:
				cairo_move_to( pContext, pPoints[0].x, pPoints[0].y );
				break;
			case k_EMovetoRel:
				cairo_rel_move_to( pContext, pPoints[0].x, pPoints[0].y );
				break;
			case k_ELinetoAbs:
				cairo_line_to( pContext, pPoints[0].x, pPoints[0].y );
				break;
			case k_ELinetoRel:
				cairo_rel_line_to( pContext, pPoints[0].x, pPoints[0].y );
				break;
			case k_EHorizontalLinetoAbs:
			{
				double current_x = 0.0;
				double current_y = 0.0;
				if( cairo_has_current_point( pContext ) ) 
				{
					cairo_get_current_point( pContext, &current_x, &current_y );
				}
				cairo_line_to( pContext, pPoints[0].x, current_y );
			}
			break;
			case k_EHorizontalLinetoRel:
				cairo_rel_line_to( pContext, pPoints[0].x, 0.0 );
				break;
			case k_EVerticalLinetoAbs:
			{
				double current_x = 0.0;
				double current_y = 0.0;
				if( cairo_has_current_point( pContext ) )
				{
					cairo_get_current_point( pContext, &current_x, &current_y );
				}
				cairo_line_to( pContext, current_x, pPoints[0].y );
			}
			break;
			case k_EVerticalLinetoRel:
				cairo_rel_line_to( pContext, 0.0, pPoints[0].y );
				break;
			case k_EClose:
				cairo_close_path( pContext );
				break;
			case k_ECubicAbs:
				cairo_curve_to( pContext, pPoints[0].x, pPoints[0].y, pPoints[1].x, pPoints[1].y, pPoints[2].x, pPoints[2].y );
				break;
			case k_ECubicRel:
				cairo_rel_curve_to( pContext, pPoints[0].x, pPoints[0].y, pPoints[1].x, pPoints[1].y, pPoints[2].x, pPoints[2].y );
				break;
			case k_ECubicSmoothRel:
			{
				// Default first control point to current point (0.0 in relative coords)
				Vector2D control1(0.0f, 0.0f);

				// If previous step was a cubic bezier, use the reflection of 
				// the previous second control point as the first control point
				if( pPrevSegment && ((pPrevSegment->m_type == k_ECubicRel) || (pPrevSegment->m_type == k_ECubicSmoothRel)))
				{
					Vector2D& prevControl2 = pPrevSegment->m_points[1];
					Vector2D& prevEnd = pPrevSegment->m_points[2];
					control1 = prevEnd - prevControl2;
				}
				else if( pPrevSegment && ((pPrevSegment->m_type == k_ECubicAbs) || (pPrevSegment->m_type == k_ECubicSmoothAbs)) )
				{
					double current_x = 0.0;
					double current_y = 0.0;
					if( cairo_has_current_point( pContext ) ) 
					{
						cairo_get_current_point( pContext, &current_x, &current_y );
					}
					Vector2D& prevControl2 = pPrevSegment->m_points[1];
					Vector2D prevEnd = Vector2D( float( current_x ), float( current_y ) );
					control1 = prevEnd - prevControl2;
				}

				cairo_rel_curve_to( pContext, control1.x, control1.y, pPoints[1].x, pPoints[1].y, pPoints[2].x, pPoints[2].y );
			}
			break;
			case k_ECubicSmoothAbs:
			{
				// Default first control point to current point
				double current_x = 0.0;
				double current_y = 0.0;
				if( cairo_has_current_point( pContext ) ) 
				{
					cairo_get_current_point( pContext, &current_x, &current_y );
				}
				Vector2D control1((float)current_x, (float)current_y);

				// If previous step was a cubic bezier, use the reflection of 
				// the previous second control point as the first control point
				if( pPrevSegment && ((pPrevSegment->m_type == k_ECubicRel) || (pPrevSegment->m_type == k_ECubicSmoothRel)) )
				{
					Vector2D& prevControl2 = pPrevSegment->m_points[1];
					Vector2D& prevEnd = pPrevSegment->m_points[2];
					control1 += prevEnd - prevControl2;
				}
				else if( pPrevSegment && ((pPrevSegment->m_type == k_ECubicAbs) || (pPrevSegment->m_type == k_ECubicSmoothAbs)) )
				{
					Vector2D& prevControl2 = pPrevSegment->m_points[1];
					Vector2D prevEnd = Vector2D( float( current_x ), float( current_y ) );
					control1 += prevEnd - prevControl2;
				}

				cairo_curve_to( pContext, control1.x, control1.y, pPoints[1].x, pPoints[1].y, pPoints[2].x, pPoints[2].y );
			}
			break;
			case k_EQuadraticAbs:
			case k_EQuadraticRel:
			case k_EQuadraticSmoothRel:
			case k_EQuadraticSmoothAbs:
			{
				// TODO: Fill this in based on:
				// https://lists.cairographics.org/archives/cairo/2010-April/019691.html
				Msg( "Don't yet support quadratic bezier rendering\n" );
			}
			break;
			case k_EArcAbs:
			case k_EArcRel:
			{
				Msg( "Don't yet support elliptical arc  rendering\n" );
			}
			break;
			default:
				Msg( "Don't support path segment type %d\n", pSegment->m_type );
				break;
			}

			pPrevSegment = pSegment;
		}

		if( bRenderForClip )
		{
			ClipCurrentShape( &pPathElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pPathElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// RectElement <rect>
	//----------------------------------------------------------------------------

	struct RectElement : public GraphicsElement
	{
		RectElement( CSvgLoader *pSvg ) : m_x(0.0f), m_y(0.0f), m_width(0.0f), m_height(0.0f), m_rx(0.0f), m_ry(0.0f), GraphicsElement(k_ERect, pSvg) {}
		virtual ~RectElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderRectElement( this, pContext ); }
		virtual void Clip( cairo_t *pContext ) { m_pSvg->RenderRectElement( this, pContext, true ); }

		float m_x;
		float m_y;
		float m_width;
		float m_height;
		float m_rx;
		float m_ry;
	};

	bool ParseRectElement( RectElement *pRectElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;
			if( V_strcmp( "x", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_x, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_y, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "width", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_width, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "height", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_height, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "rx", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_rx, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "ry", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRectElement->m_ry, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pRectElement, atts );
	}

	void RenderRectElement( RectElement *pRectElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pRectElement, renderState, bRenderForClip );

		cairo_rectangle( pContext, pRectElement->m_x, pRectElement->m_y, pRectElement->m_width, pRectElement->m_height );

		// TODO: Rounded corners

		if( bRenderForClip )
		{
			ClipCurrentShape( &pRectElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pRectElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// CircleElement <circle>
	//----------------------------------------------------------------------------

	struct CircleElement : public GraphicsElement 
	{
		CircleElement( CSvgLoader *pSvg ) : m_cx( 0.0f ), m_cy( 0.0f ), m_r( 0.0f ), GraphicsElement( k_ECircle, pSvg ) {}
		virtual ~CircleElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderCircleElement( this, pContext ); }
		virtual void Clip( cairo_t *pContext ) { m_pSvg->RenderCircleElement( this, pContext, true ); }

		float m_cx;
		float m_cy;
		float m_r;
	};

	bool ParseCircleElement( CircleElement *pCircleElement, LPXMLVECTOR atts ) 
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;
			if( V_strcmp( "cx", pchAttrName ) == 0 )
			{
				if ( !SVGHelpers::BParseFloat( &pCircleElement->m_cx, pchAttrValue ) ) 
				{
					return false;
				}
			}
			else if ( V_strcmp( "cy", pchAttrName ) == 0 ) 
			{
				if ( !SVGHelpers::BParseFloat( &pCircleElement->m_cy, pchAttrValue ) )
				{
					return false;
				}
			}
			else if ( V_strcmp( "r", pchAttrName ) == 0 ) 
			{
				if ( !SVGHelpers::BParseFloat( &pCircleElement->m_r, pchAttrValue ) )
				{
					return false;
				}
			}		 
		}

		return ParseGraphicsElement( pCircleElement, atts );
	}

	void RenderCircleElement( CircleElement *pCircleElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pCircleElement, renderState, bRenderForClip );

		cairo_arc( pContext, pCircleElement->m_cx, pCircleElement->m_cy, pCircleElement->m_r, 0.0, M_PI*2.0 );

		if( bRenderForClip )
		{
			ClipCurrentShape( &pCircleElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pCircleElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// EllipseElement <ellipse>
	//----------------------------------------------------------------------------

	struct EllipseElement : public GraphicsElement
	{
		EllipseElement( CSvgLoader *pSvg ) : m_cx( 0.0f ), m_cy( 0.0f ), m_rx( 0.0f ), m_ry( 0.0f ), GraphicsElement( k_EEllipse, pSvg ) {}
		virtual ~EllipseElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderEllipseElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderEllipseElement( this, pContext, true ); }

		float m_cx;
		float m_cy;
		float m_rx;
		float m_ry;
	};

	bool ParseEllipseElement( EllipseElement *pEllipseElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;
			if( V_strcmp( "cx", pchAttrName ) == 0 ) 
			{
				if( !SVGHelpers::BParseFloat( &pEllipseElement->m_cx, pchAttrValue ) ) 
				{
					return false;
				}
			}
			else if( V_strcmp( "cy", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pEllipseElement->m_cy, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "rx", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pEllipseElement->m_rx, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "ry", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pEllipseElement->m_ry, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pEllipseElement, atts );
	}

	void RenderEllipseElement( EllipseElement *pEllipseElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pEllipseElement, renderState, bRenderForClip );

		cairo_save( pContext );
		cairo_translate( pContext, pEllipseElement->m_cx, pEllipseElement->m_cy );
		cairo_scale( pContext, pEllipseElement->m_rx, pEllipseElement->m_ry );
		cairo_arc( pContext, 0.0, 0.0, 1.0, 0.0, M_PI*2.0 );
		cairo_restore( pContext );

		if( bRenderForClip )
		{
			ClipCurrentShape( &pEllipseElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pEllipseElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// LineElement <line>
	//----------------------------------------------------------------------------

	struct LineElement : public GraphicsElement
	{
		LineElement( CSvgLoader *pSvg ) : m_x1( 0.0f ), m_y1( 0.0f ), m_x2( 0.0f ), m_y2( 0.0f ), GraphicsElement( k_ELine, pSvg ) {}
		virtual ~LineElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderLineElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderLineElement( this, pContext, true ); }

		float m_x1;
		float m_y1;
		float m_x2;
		float m_y2;
	};

	bool ParseLineElement( LineElement *pLineElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;
			if( V_strcmp( "x1", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLineElement->m_x1, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y1", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLineElement->m_y1, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "x2", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLineElement->m_x2, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y2", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLineElement->m_y2, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pLineElement, atts );
	}

	void RenderLineElement( LineElement *pLineElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pLineElement, renderState, bRenderForClip );

		cairo_move_to( pContext, pLineElement->m_x1, pLineElement->m_y1 );
		cairo_line_to( pContext, pLineElement->m_x2, pLineElement->m_y2 );

		if( bRenderForClip )
		{
			ClipCurrentShape( &pLineElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pLineElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// PolylineElement <polyline>
	//----------------------------------------------------------------------------

	struct PolylineElement : public GraphicsElement
	{
		PolylineElement( CSvgLoader *pSvg ) : GraphicsElement( k_EPolyline, pSvg ) {}
		virtual ~PolylineElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderPolylineElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderPolylineElement( this, pContext, true ); }

		CUtlVector<Vector2D> m_vecPoints;
	};

	bool ParsePolyline( CUtlVector<Vector2D> *pVecPoints, const char* pchAttrValue )
	{
		const char* pVal = pchAttrValue;

		pVal = SVGHelpers::SkipSpaces( pVal );

		while( *pVal )
		{
			int iPoint = pVecPoints->AddToTail();
			Vector2D& point = pVecPoints->Element( iPoint );

			const char* pEnd;
			if( !SVGHelpers::BParseFloat( &point.x, pVal, &pEnd ) )
			{
				return false;
			}
			pVal = pEnd;
			pVal = SVGHelpers::SkipSpacesOrComma( pVal );
			if( !SVGHelpers::BParseFloat( &point.y, pVal, &pEnd ) )
			{
				return false;
			}
			pVal = pEnd;
			pVal = SVGHelpers::SkipSpacesOrComma( pVal );
		}
		return true;
	}

	bool ParsePolylineElement( PolylineElement *pPolylineElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;

			if( V_strcmp( "points", pchAttrName ) == 0 )
			{
				if( !ParsePolyline( &pPolylineElement->m_vecPoints, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pPolylineElement, atts );
	}

	void RenderPolylineElement( PolylineElement *pPolylineElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		if( pPolylineElement->m_vecPoints.Count() == 0 )
		{
			return;
		}

		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pPolylineElement, renderState, bRenderForClip );

		FOR_EACH_VEC( pPolylineElement->m_vecPoints, i )
		{
			Vector2D* pPoint = &pPolylineElement->m_vecPoints[i];

			if( i == 0 )
			{
				cairo_move_to( pContext, pPoint->x, pPoint->y );
			}
			else
			{
				cairo_line_to( pContext, pPoint->x, pPoint->y );
			}
		}

		if( bRenderForClip )
		{
			ClipCurrentShape( &pPolylineElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pPolylineElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// PolygonElement <polygon>
	//----------------------------------------------------------------------------

	struct PolygonElement : public GraphicsElement
	{
		PolygonElement( CSvgLoader *pSvg ) : GraphicsElement( k_EPolygon, pSvg ) {}
		virtual ~PolygonElement() {}

		virtual void Render( cairo_t* pContext ) { m_pSvg->RenderPolygonElement( this, pContext ); }
		virtual void Clip( cairo_t* pContext ) { m_pSvg->RenderPolygonElement( this, pContext, true ); }

		CUtlVector<Vector2D> m_vecPoints;
	};

	bool ParsePolygonElement( PolygonElement *pPolygonElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;

			if( V_strcmp( "points", pchAttrName ) == 0 )
			{
				if( !ParsePolyline( &pPolygonElement->m_vecPoints, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGraphicsElement( pPolygonElement, atts );
	}

	void RenderPolygonElement( PolygonElement *pPolygonElement, cairo_t* pContext, bool bRenderForClip = false )
	{
#ifdef CAIRO_ENABLED
		if( pPolygonElement->m_vecPoints.Count() == 0 )
		{
			return;
		}

		GraphicsElementRenderState renderState( pContext );
		PreRenderGraphicsElement( pPolygonElement, renderState, bRenderForClip );

		FOR_EACH_VEC( pPolygonElement->m_vecPoints, i )
		{
			Vector2D* pPoint = &pPolygonElement->m_vecPoints[i];

			if( i == 0 )
			{
				cairo_move_to( pContext, pPoint->x, pPoint->y );
			}
			else
			{
				cairo_line_to( pContext, pPoint->x, pPoint->y );
			}
		}
		cairo_close_path( pContext );

		if( bRenderForClip )
		{
			ClipCurrentShape( &pPolygonElement->m_presentationAttributes, pContext );
		}
		else
		{
			RenderCurrentShape( &pPolygonElement->m_presentationAttributes, pContext );
		}

		PostRenderGraphicsElement( renderState, bRenderForClip );
#endif
	}

	//----------------------------------------------------------------------------
	// StopElement <stop>
	// A stop element must be a child of a gradient element. 
	//----------------------------------------------------------------------------
	struct StopElement : public Element
	{
		StopElement( CSvgLoader *pSvg ) : Element( k_EStop, pSvg ), m_offset(0.0f), m_color(0,0,0,1), m_fOpacity(1.0f) {}
		virtual ~StopElement() {}

		float m_offset;
		Color m_color;
		float m_fOpacity;
	};

	// Parse CSS-style stop attributes, e.g. "stop-opacity:0.5;stop-color:#0E0F0F;"
	bool ParseCssStopAttributes( StopElement *pStopElement, const char* pchStyles )
	{
		const char* pTmp = pchStyles;
		pTmp = SVGHelpers::SkipSpaces( pTmp );
		char pchName[128];
		char pchValue[128];
		const char* pEnd = NULL;
		while( *pTmp )
		{
			// Read attribute name
			if( !SVGHelpers::BParseCSSToken( pchName, V_ARRAYSIZE( pchName ), pTmp, &pEnd ) )
			{
				return false;
			}
			pTmp = pEnd;
			pTmp = SVGHelpers::SkipSpaces( pTmp );

			if( *pTmp != ':' )
			{
				return false;
			}
			++pTmp;

			// Read attribute value
			if( !SVGHelpers::BParseCSSToken( pchValue, V_ARRAYSIZE( pchValue ), pTmp, &pEnd ) )
			{
				return false;
			}
			pTmp = pEnd;
			pTmp = SVGHelpers::SkipSpaces( pTmp );
			if( *pTmp == ';' )
			{
				++pTmp;
			}

			if( V_strcmp( "stop-color", pchName ) == 0 )
			{
				if( !SVGHelpers::BParseColor( &pStopElement->m_color, pchValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "stop-opacity", pchName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pStopElement->m_fOpacity, pchValue ) )
				{
					return false;
				}
			}
		}
		return true;
	}

	bool ParseStopElement( StopElement *pStopElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;

			if( V_strcmp( "offset", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pStopElement->m_offset, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "stop-color", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseColor( &pStopElement->m_color, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "stop-opacity", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pStopElement->m_fOpacity, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "style", pchAttrName ) == 0 )
			{
				if( !ParseCssStopAttributes( pStopElement, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return AddStopElement( pStopElement );
	}

	bool AddStopElement( StopElement* pElement )
	{
		if( !m_vecCurrentElementStack.Count() )
		{
			Msg( "Error: Attempting to add stop element before parsing SVG root\n" );
			return false;
		}

		GradientElement *pParent = dynamic_cast<GradientElement *>(m_vecCurrentElementStack.Tail());
		if( !pParent )
		{
			Msg( "Error: Attempting to add stop element to a non-gradient element\n" );
			return false;
		}
		pParent->m_vecChildren.AddToTail( pElement );
		pElement->m_pParent = pParent;

		// this panel is now top of the stack
		m_vecCurrentElementStack.AddToTail( pElement );
		return true;
	}

	//----------------------------------------------------------------------------
	// GradientElement
	// A common base class for linear and radial gradients. 
	//----------------------------------------------------------------------------
	struct GradientElement : public ContainerElement
	{
		GradientElement( ESvgXMLElements type, CSvgLoader *pSvg ) : ContainerElement( type, pSvg ), m_flags(0) {}

		virtual void Render( cairo_t* pContext ) {}  // Gradient elements are not rendered directly
		virtual void StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue ) {} // Not applicable for gradients

		uint32 m_flags;
		CUtlVector<Transform> m_vecTransforms;
	};

	bool ParseGradient( GradientElement *pGradient, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;
			if( V_strcmp( "gradientTransform", pchAttrName ) == 0 )
			{
				if( !ParseTransforms( &pGradient->m_vecTransforms, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		//ParsePresentationAttributes( &pGradient->m_presentationAttributes, atts ); Is this required for gradients?
		ParseElement( pGradient, atts );

		return AddChildElement( pGradient );
	}

	void ApplyGradient( GradientElement *pGradient, cairo_t *pContext )
	{
#ifdef CAIRO_ENABLED
		// TODO: Handle gradient units set to bounding box
		
		TransformState tState;
		SaveAndApplyTransforms( &pGradient->m_vecTransforms, tState, pContext );

		cairo_pattern_t* pPattern = NULL;
		if( pGradient->m_type == k_ELinearGradient )
		{
			LinearGradientElement *pLinearGradientElement = (LinearGradientElement*)pGradient;
			Vector2D* pPoints = pLinearGradientElement->m_points;
			pPattern = cairo_pattern_create_linear( pPoints[0].x, pPoints[0].y, pPoints[1].x, pPoints[1].y );

		}
		else if( pGradient->m_type == k_ERadialGradient )
		{
			RadialGradientElement *pRadialGradientElement = (RadialGradientElement*)pGradient;
			Vector2D* pCentre = &pRadialGradientElement->m_centre;
			Vector2D* pFocalPoint = &pRadialGradientElement->m_focalPoint;
			pPattern = cairo_pattern_create_radial( pFocalPoint->x, pFocalPoint->y, 0, pCentre->x, pCentre->y, pRadialGradientElement->m_radius );
		}
		else
		{
			Msg( "Warning: unknown gradient type\n" );
		}
		FOR_EACH_VEC( pGradient->m_vecChildren, i )
		{
			if( pGradient->m_vecChildren[i]->m_type == k_EStop )
			{
				StopElement* pStop = (StopElement*)pGradient->m_vecChildren[i];
				float r = (float)pStop->m_color.r() / 255.0f;
				float g = (float)pStop->m_color.g() / 255.0f;
				float b = (float)pStop->m_color.b() / 255.0f;
				cairo_pattern_add_color_stop_rgba( pPattern, pStop->m_offset, b, g, r, pStop->m_fOpacity ); //Swap red and blue
			}
		}

		// TODO: Call cairo_pattern_set_extend based on spread method

		if( pPattern )
		{
			cairo_set_source( pContext, pPattern );
			cairo_pattern_destroy( pPattern );
		}

		RestoreTransforms( tState, pContext );
#endif
	}

	//----------------------------------------------------------------------------
	// LinearGradientElement <linearGradient>
	//----------------------------------------------------------------------------
	struct LinearGradientElement : public GradientElement
	{
		LinearGradientElement( CSvgLoader *pSvg ) : GradientElement( k_ELinearGradient, pSvg ) { m_points[0].Init( 0.0f, 0.0f ); m_points[1].Init( 100.0f, 0.0f ); }
		virtual ~LinearGradientElement() {}
		Vector2D m_points[2];
	};

	bool ParseLinearGradientElement( LinearGradientElement *pLinearGradientElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;

			if( V_strcmp( "x1", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLinearGradientElement->m_points[0].x, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y1", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLinearGradientElement->m_points[0].y, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "x2", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLinearGradientElement->m_points[1].x, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "y2", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pLinearGradientElement->m_points[1].y, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		return ParseGradient( pLinearGradientElement, atts );
	}

	//----------------------------------------------------------------------------
	// RadialGradientElement <radialGradient>
	//----------------------------------------------------------------------------
	struct RadialGradientElement : public GradientElement
	{
		RadialGradientElement( CSvgLoader *pSvg ) : GradientElement( k_ERadialGradient, pSvg ), m_radius( 0.0f ) { m_centre.Init( 50.0f, 50.0f ); m_focalPoint.Init( 0.0f, 0.0f ); }
		virtual ~RadialGradientElement() {}
		Vector2D m_centre;
		float m_radius;
		Vector2D m_focalPoint;
	};

	bool ParseRadialGradientElement( RadialGradientElement *pRadialGradientElement, LPXMLVECTOR atts )
	{
		for( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char*)att->qname;
			const char *pchAttrValue = (const char*)att->value;

			if( V_strcmp( "cx", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRadialGradientElement->m_centre.x, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "cy", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRadialGradientElement->m_centre.y, pchAttrValue ) )
				{
					return false;
				}
			}
			else if( V_strcmp( "r", pchAttrName ) == 0 )
			{
				if( !SVGHelpers::BParseFloat( &pRadialGradientElement->m_radius, pchAttrValue ) )
				{
					return false;
				}
			}
		}

		// Parse focal point, default to centre point if not set
		const char* pVal = GetAttVal( atts, "fx" );
		if( pVal )
		{
			if( !SVGHelpers::BParseFloat( &pRadialGradientElement->m_focalPoint.x, pVal ) )
			{
				return false;
			}
		}
		else
		{
			pRadialGradientElement->m_focalPoint.x = pRadialGradientElement->m_centre.x;
		}
		pVal = GetAttVal( atts, "fy" );
		if( pVal )
		{
			if( !SVGHelpers::BParseFloat( &pRadialGradientElement->m_focalPoint.y, pVal ) )
			{
				return false;
			}
		}
		else
		{
			pRadialGradientElement->m_focalPoint.y = pRadialGradientElement->m_centre.y;
		}

		return ParseGradient( pRadialGradientElement, atts );
	}

};

void CSvgLoader::StyleOverride( ESvgAttribute attributeType, const SvgAttributeValue_t* pAttributeValue )
{
	if( m_pRootElement )
	{
		StyleOverrideContainer( m_pRootElement, attributeType, pAttributeValue );
	}
}

bool CSvgLoader::Render( CUtlBuffer &bufOutput, uint32& width, uint32& height )
{
	if( !m_pRootElement )
	{
		return false;
	}

	float fFileWidth = m_pRootElement->m_width.m_val;
	float fFileHeight = m_pRootElement->m_height.m_val;

	if( width == 0 && height == 0 )
	{
		width = fFileWidth;
		height = fFileHeight;
	}
	else
	{
		if( width == 0 )
		{
			width = fFileWidth;
			if( fFileHeight > 0.0f )
			{
				// preserve aspect ratio
				width = (uint32)((float)width * height / fFileHeight);
			}
		}

		if( height == 0 )
		{
			height = fFileHeight;
			if( fFileWidth > 0.0f )
			{
				// preserve aspect ratio
				height = (uint32)((float)height * width / fFileWidth);
			}
		}
	}
	
	Assert( width != 0 );
	Assert( height != 0 );
	if( (width == 0) || (height == 0) )
	{
		return false;
	}

	int outputBytes = width * height * sizeof( uint32 );
	bufOutput.EnsureCapacity( outputBytes );
	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, outputBytes );
	V_memset( bufOutput.Base(), 0, outputBytes );

#ifdef CAIRO_ENABLED
	cairo_surface_t* pSurface = cairo_image_surface_create_for_data(
		(unsigned char*)bufOutput.Base(),
		CAIRO_FORMAT_ARGB32,
		width,
		height,
		width * sizeof( uint32 )
	);
	cairo_t* pContext = cairo_create( pSurface );

	cairo_scale( pContext, double( width ) / double( fFileWidth ), double( height ) / double( fFileHeight ) );

	// Clear surface
	cairo_set_source_rgba( pContext, 0.0, 0.0, 0.0, 0.0 );
	cairo_set_operator( pContext, CAIRO_OPERATOR_SOURCE );
	cairo_paint( pContext );
	cairo_set_operator( pContext, CAIRO_OPERATOR_OVER );

	RenderRootElement( m_pRootElement, pContext );

	cairo_destroy( pContext );
	cairo_surface_destroy( pSurface );
	cairo_debug_reset_static_data();
	return true;
#endif
	return false;
}


//-------------------------------------------------------------------------------------------------
// Parsifal callbacks
//-------------------------------------------------------------------------------------------------

int CSvgLoader::StartElement( const XMLCH *uri, const char *localName, const char *pchName, LPXMLVECTOR atts )
{
	bool bParseResult = true;

	int elementType = ElementTypeFromString( pchName );
	if( m_eCurrentElement == k_EInvalid )
	{
		if( elementType == k_ESvg )
		{
			m_eCurrentElement = k_ESvg;
			m_pRootElement = new RootElement( this );
			m_pCurrentElement = m_pRootElement;
			bParseResult = ParseRootElement( m_pRootElement, atts );
		}
	}
	else if( !m_bParseRootElementOnly )
	{
		switch( elementType )
		{
		case k_EGroup:
			m_eCurrentElement = k_EGroup;
			m_pCurrentElement = new GroupElement( this );
			bParseResult = ParseGroupElement( (GroupElement*)m_pCurrentElement, atts );
			break;
		case k_EDefs:
			m_eCurrentElement = k_EDefs;
			m_pCurrentElement = new DefsElement( this );
			bParseResult = ParseDefsElement( (DefsElement*)m_pCurrentElement, atts );
			break;
		case k_ESymbol:
			m_eCurrentElement = k_ESymbol;
			m_pCurrentElement = new SymbolElement( this );
			bParseResult = ParseSymbolElement( (SymbolElement*)m_pCurrentElement, atts );
			break;
		case k_EUse:
			m_eCurrentElement = k_EUse;
			m_pCurrentElement = new UseElement( this );
			bParseResult = ParseUseElement( (UseElement*)m_pCurrentElement, atts );
			break;
		case k_EClipPath:
			m_eCurrentElement = k_EClipPath;
			m_pCurrentElement = new ClipPathElement( this );
			bParseResult = ParseClipPathElement( (ClipPathElement*)m_pCurrentElement, atts );
			break;
		case k_EPath:
			m_eCurrentElement = k_EPath;
			m_pCurrentElement = new PathElement( this );
			bParseResult = ParsePathElement( (PathElement*)m_pCurrentElement, atts );
			break;
		case k_ELinearGradient:
			m_eCurrentElement = k_ELinearGradient;
			m_pCurrentElement = new LinearGradientElement( this );
			bParseResult = ParseLinearGradientElement( (LinearGradientElement*)m_pCurrentElement, atts );
			break;
		case k_ERadialGradient:
			m_eCurrentElement = k_ERadialGradient;
			m_pCurrentElement = new RadialGradientElement( this );
			bParseResult = ParseRadialGradientElement( (RadialGradientElement*)m_pCurrentElement, atts );
			break;
		case k_EStop:
			m_eCurrentElement = k_EStop;
			m_pCurrentElement = new StopElement( this );
			bParseResult = ParseStopElement( (StopElement*)m_pCurrentElement, atts );
			break;
		case k_ERect:
			m_eCurrentElement = k_ERect;
			m_pCurrentElement = new RectElement( this );
			bParseResult = ParseRectElement( (RectElement*)m_pCurrentElement, atts );
			break;
		case k_ECircle:
			m_eCurrentElement = k_ECircle;
			m_pCurrentElement = new CircleElement( this );
			bParseResult = ParseCircleElement( (CircleElement*)m_pCurrentElement, atts );
			break;
		case k_EEllipse:
			m_eCurrentElement = k_EEllipse;
			m_pCurrentElement = new EllipseElement( this );
			bParseResult = ParseEllipseElement( (EllipseElement*)m_pCurrentElement, atts );
			break;
		case k_ELine:
			m_eCurrentElement = k_ELine;
			m_pCurrentElement = new LineElement( this );
			bParseResult = ParseLineElement( (LineElement*)m_pCurrentElement, atts );
			break;
		case k_EPolyline:
			m_eCurrentElement = k_EPolyline;
			m_pCurrentElement = new PolylineElement( this );
			bParseResult = ParsePolylineElement( (PolylineElement*)m_pCurrentElement, atts );
			break;
		case k_EPolygon:
			m_eCurrentElement = k_EPolygon;
			m_pCurrentElement = new PolygonElement( this );
			bParseResult = ParsePolygonElement( (PolygonElement*)m_pCurrentElement, atts );
			break;
		default:
			break;
		}
	}

	if( !bParseResult )
	{
		return XML_ABORT;
	}
	return XML_OK;
}

int CSvgLoader::CDATAStart()
{
	return XML_OK;
}

int CSvgLoader::Characters( const char *Chars, int cbChars )
{
	return XML_OK;
}

void CSvgLoader::GetCurrentParsePosition( int *pLineOut, int *pColOut )
{
	*pLineOut = XMLParser_GetCurrentLine( m_parser );
	*pColOut = XMLParser_GetCurrentColumn( m_parser );
}

int CSvgLoader::ParseError( const char *pchMsg, ... )
{
	va_list args;
	va_start( args, pchMsg );

	m_strAbortError.FormatV( pchMsg, args );

	return XML_ABORT;
}

int CSvgLoader::EndElement( const XMLCH *uri, const char *localName, const char *pchName )
{
	if( m_eCurrentElement != k_EInvalid ) // Ignore elements outside the root SVG element
	{
		int elementType = ElementTypeFromString( pchName );

		if( m_bParseRootElementOnly )
		{
			if( elementType != k_ESvg )
			{
				return XML_OK;
			}
		}

		switch( elementType )
		{
		case k_ESvg:
			m_eCurrentElement = k_EInvalid;
			m_pCurrentElement = NULL;
			m_vecCurrentElementStack.RemoveAll();
			break;
		case k_EGroup:
		case k_EPath:
		case k_ECircle:
		case k_EEllipse:
		case k_ERect:
		case k_ELine:
		case k_EPolygon:
		case k_EPolyline:
		case k_EDefs:
		case k_ESymbol:
		case k_EUse:
		case k_ELinearGradient:
		case k_ERadialGradient:
		case k_EStop:
		case k_EClipPath:
			if( m_vecCurrentElementStack.Count() == 0 )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error - popping panel off empty stack: line %d, col %d", nLine, nCol );
			}
			// pop panel from stack
			m_vecCurrentElementStack.Remove( m_vecCurrentElementStack.Count() - 1 );

			// out of panels?
			if( m_vecCurrentElementStack.Count() == 0 )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error - popping last remaining panel off stack, but not root element: line %d, col %d", nLine, nCol );
			}
			else
			{
				m_pCurrentElement = m_vecCurrentElementStack.Tail();
				m_eCurrentElement = m_pCurrentElement->m_type;
			}
			break;
		default:
			break;
		}
	}

	return XML_OK;
}

int CSvgLoader::cstream( BYTE *buf, int cBytes, int *cBytesActual )
{
	if( m_svgDataSize == m_currentDataPos )
	{
		*cBytesActual = 0;
	}
	else
	{
		if( cBytes > (m_svgDataSize - m_currentDataPos) )
		{
			*cBytesActual = m_svgDataSize - m_currentDataPos;
		}
		else
		{
			*cBytesActual = cBytes;
		}
		V_memcpy( buf, m_pSvgData + m_currentDataPos, *cBytesActual );
		m_currentDataPos += *cBytesActual;
	}
	return (*cBytesActual < cBytes);
}

static int StaticCstream( BYTE *buf, int cBytes, int *cBytesActual, void *inputData )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)inputData;
	return pSvgLoader->cstream( buf, cBytes, cBytesActual );
}

static int StaticStartElement( void *pUserData, const XMLCH *uri, const XMLCH *localName, const XMLCH *qName, LPXMLVECTOR atts )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)pUserData;
	return pSvgLoader->StartElement( uri, (const char*)localName, (const char*)qName, atts );
}

static int StaticCharacters( void *pUserData, const XMLCH *Chars, int cbChars )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)pUserData;
	return pSvgLoader->Characters( (const char*)Chars, cbChars );
}

static int StaticStartCDATA( void *pUserData )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)pUserData;
	return pSvgLoader->CDATAStart();
}

static int StaticEndElement( void *pUserData, const XMLCH *uri, const XMLCH *localName, const XMLCH *qName )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)pUserData;
	return pSvgLoader->EndElement( uri, (const char*)localName, (const char*)qName );
}

static void StaticErrorHandler( LPXMLPARSER parser )
{
	CSvgLoader *pSvgLoader = (CSvgLoader*)parser->UserData;
	if( pSvgLoader->m_strAbortError.IsEmpty() )
	{
		pSvgLoader->m_strAbortError = (const char*)parser->ErrorString;
	}
}

// For now assume width and height are supplied in pixels
float CSvgLoader::GetWidth() 
{ 
	if( !m_pRootElement )
	{
		return 0.0f;
	}
	return m_pRootElement->m_width.m_val;
}

float CSvgLoader::GetHeight()
{
	if( !m_pRootElement )
	{
		return 0.0f;
	}
	return m_pRootElement->m_height.m_val;
}

bool CSvgLoader::Parse()
{
	if( !XMLParser_Create( &m_parser ) )
		return false;

	//set callback functions
	m_parser->errorHandler = StaticErrorHandler;
	m_parser->startElementHandler = StaticStartElement;
	m_parser->endElementHandler = StaticEndElement;
	m_parser->charactersHandler = StaticCharacters;
	m_parser->ignorableWhitespaceHandler = StaticCharacters;
	m_parser->startCDATAHandler = StaticStartCDATA;
	m_parser->UserData = this;

	// parse buffer
	XMLParser_Parse( m_parser, StaticCstream, this, NULL );
	XMLParser_Free( m_parser );

	if( !m_strAbortError.IsEmpty() )
	{
		Msg( "Error parsing SVG: %s\n", m_strAbortError.String() );
		return false;
	}

	return true;
}

bool CSvgLoader::ParseRootElementOnly()
{
	m_bParseRootElementOnly = true;
	return Parse();
}

// Get dimensions out of SVG header
bool GetSVGDimensions( const byte *pubSVGData, int cubSVGData, uint32 &width, uint32 &height )
{
	CSvgLoader svgLoader( pubSVGData, cubSVGData );
	if( svgLoader.ParseRootElementOnly() )
	{
		uint32 w = width;
		uint32 h = height;

		if( !w )
		{
			w = svgLoader.GetWidth();

			if( height && svgLoader.GetHeight() )
			{
				// preserve aspect ratio
				w = (uint32)((float)w * height / svgLoader.GetHeight());
			}
		}

		if( !h )
		{
			h = svgLoader.GetHeight();

			if( width && svgLoader.GetWidth() )
			{
				// preserve aspect ratio
				h = (uint32)((float)h * width / svgLoader.GetWidth());
			}
		}

		width = w;
		height = h;

		return true;
	}
	return false;
}

// given a SVG formatted memory buffer return a raw rgba buffer
bool ConvertSVGToRGBA( const byte *pubSVGData, int cubSVGData, CUtlBuffer &bufOutput, int &width, int &height, float fScaleFactor /*=-1.0f*/, const SvgAttributeOverrides_t* pAttributeOverrides /*= NULL*/ )
{
	uint32 w = width;
	uint32 h = height;
	CSvgLoader svgLoader( pubSVGData, cubSVGData );
	if( !svgLoader.Parse() )
	{
		return false;
	}

	// attribute override
	if( pAttributeOverrides )
	{
		for( int i = 0; i < k_ESvgAttributeMax; ++i )
		{
			if( pAttributeOverrides->m_nFlags & (1 << i) )
			{
				svgLoader.StyleOverride( (ESvgAttribute)i, &pAttributeOverrides->m_overrides[i] );
			}
		}
	}

	if( fScaleFactor > 0 )
	{
		w = w*fScaleFactor;
		h = h*fScaleFactor;
	}

	// render
	svgLoader.Render( bufOutput, w, h );

	width = w;
	height = h;

	// unpremultiply alpha
	unsigned char* pRgba = (unsigned char*)bufOutput.Base();
	for( int i = 0; i < (width*height); ++i, pRgba += 4 )
	{
		unsigned char a = pRgba[3];
		if( a == 255 )
		{
			continue;
		}
		if( a != 0 )
		{
			pRgba[0] = ((int)pRgba[0] * 255) / a;
			pRgba[1] = ((int)pRgba[1] * 255) / a;
			pRgba[2] = ((int)pRgba[2] * 255) / a;
		}
	}
	return true;
}

