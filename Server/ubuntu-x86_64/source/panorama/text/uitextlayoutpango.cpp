//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include "stdafx.h"
#include "uitextlayoutpango.h"
#include "uifontfileloaderlinux.h"
#include "uienginesdl.h"
#include "language.h"

#if !defined( SOURCE2_PANORAMA )
#include "../renderer/sdlopenglsurface.h"
#else
#include "../source2/renderer/source2surface.h"
#include "filesystem.h"
#endif

#if defined( POSIX )
#include <dlfcn.h>
#endif

#if defined( LINUX )
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//#define LINEDEBUG

#ifdef LINEDEBUG
static int s_idLineDebug;
#endif

static CThreadMutex s_PangoMutex;

PangoFontMap *CUITextLayoutPango::s_pFontmap = NULL;
PangoContext *CUITextLayoutPango::s_pContext = NULL;
int CUITextLayoutPango::s_nInitializeCount = 0;
CThreadMutex CUITextLayoutPango::s_GlobalMutex;
int CUITextLayoutPango::s_nZeroFillBufferSize = 0;
const void* CUITextLayoutPango::s_pZeroFillBuffer = NULL;
bool CUITextLayoutPango::s_bDebugFontSelection = false;
PangoWrapMode CUITextLayoutPango::s_eWordWrapMode = PANGO_WRAP_WORD;

// Freetype uses a lot of fixed float values that are 26.6 splits of a 32 bit word.
#define FREETYPE_SCALE 64

#if defined( LINUX )

//
// Under Linux we need to de-conflict our custom pango from the system provided one. We do this
//  by manually loading the pango library we ship, and have that pango library statically link
//  all the symbols it needs.
// The code below is the typedefs for each of the pango functions we use plus the code to dlsym()
//  that function pointer in as needed.
//
// If you have a link error on Linux trying to use a new pango function then you will need to add a
// new WRAP_PANGO_FUNC(), #define and LOAD_PANGO_PROC below to import it for linux.
//

void *s_hPangoModule = NULL;
void *s_hPangoFT2Module = NULL;

typedef FT_Error ( *PangoFT2NewFaceOverrideFunc )( FT_Library, const char *, FT_Long, FT_Face * );
typedef FT_Error ( *FontConfigFT2NewFaceOverrideFunc )( FT_Library, const char *, FT_Long, FT_Face * );

// under Linux we load both system and our hand built freetype and fontconfig, so make sure
// we route to our one with this code
typedef FT_Error ( *PFNFT_New_Memory_Face )( FT_Library , const FT_Byte*  , FT_Long , FT_Long  , FT_Face * );
typedef FT_Error ( *PFNFT_Load_Glyph )( FT_Face   face, FT_UInt   glyph_index, FT_Int32  load_flags );
typedef FT_UInt ( *PFNFT_Get_Char_Index )( FT_Face   face, FT_ULong  charcode );
typedef FT_Error ( *PFNFT_Get_Kerning )( FT_Face     face, FT_UInt     left_glyph, FT_UInt     right_glyph, FT_UInt     kern_mode, FT_Vector  *akerning );
typedef FcBool ( *PFNFcConfigAppFontAddFile)(FcConfig    *, const FcChar8  *);
typedef FcBool ( *PFNFcConfigAppFontAddDir )(FcConfig    *, const FcChar8  *);
typedef void ( *PFNpango_ft2_new_face_substitute)( PangoFT2NewFaceOverrideFunc  func );
typedef void ( *PFNfontconfig_ft2_new_face_substitute)( FontConfigFT2NewFaceOverrideFunc  func );
typedef void ( *PFNg_object_unref )(gpointer object);
typedef void ( *PFNg_free )(gpointer mem);
typedef gboolean ( *PFNg_type_check_instance_is_a )(GTypeInstance *type_instance, GType iface_type);
typedef GTypeInstance* ( *PFNg_type_check_instance_cast )(GTypeInstance *type_instance, GType iface_type);

PFNFT_New_Memory_Face s_pfnFT_New_Memory_Face;
PFNFT_Load_Glyph s_pfnFT_Load_Glyph;
PFNFT_Get_Char_Index s_pfnFT_Get_Char_Index;
PFNFT_Get_Kerning s_pfnFT_Get_Kerning;
PFNFcConfigAppFontAddFile s_pfnFcConfigAppFontAddFile;
PFNFcConfigAppFontAddDir s_pfnFcConfigAppFontAddDir;
PFNpango_ft2_new_face_substitute s_pfnpango_ft2_new_face_substitute;
PFNfontconfig_ft2_new_face_substitute s_pfnfontconfig_ft2_new_face_substitute;
PFNg_object_unref s_pfng_object_unref;
PFNg_free s_pfng_free;
PFNg_type_check_instance_is_a s_pfng_type_check_instance_is_a;
PFNg_type_check_instance_cast s_pfng_type_check_instance_cast;

#define FT_New_Memory_Face s_pfnFT_New_Memory_Face
#define FT_Load_Glyph s_pfnFT_Load_Glyph
#define FT_Get_Char_Index s_pfnFT_Get_Char_Index
#define FT_Get_Kerning s_pfnFT_Get_Kerning
#define FcConfigAppFontAddFile s_pfnFcConfigAppFontAddFile
#define FcConfigAppFontAddDir s_pfnFcConfigAppFontAddDir
#define pango_ft2_new_face_substitute s_pfnpango_ft2_new_face_substitute
#define fontconfig_ft2_new_face_substitute s_pfnfontconfig_ft2_new_face_substitute
#define g_object_unref s_pfng_object_unref
#define g_free s_pfng_free
#define g_type_check_instance_is_a s_pfng_type_check_instance_is_a
#define g_type_check_instance_cast s_pfng_type_check_instance_cast 

#define WRAP_PANGO_FUNC( retval, args, name ) \
	typedef retval (*PFN##name) args; \
	static PFN##name s_pfn##name;

#define LOAD_PANGO_PROC( name ) \
	name = (PFN##name)dlsym( s_hPangoModule, #name ); \
	Assert( name );

#define LOAD_PANGO_FT2_PROC( name ) \
	name = (PFN##name)dlsym( s_hPangoFT2Module, #name ); \
	Assert( name );


//
// This is a list of all the pango functions we import, the macro here helps automatically emit the typedef and static symbol for the function
//
//
WRAP_PANGO_FUNC( PangoFontMap *, (void), pango_ft2_font_map_new )
WRAP_PANGO_FUNC( void, (FT_Bitmap  *bitmap,  const PangoMatrix *matrix, PangoFont *font,  PangoGlyphString *glyphs,  int x, int  y), pango_ft2_render_transformed )

WRAP_PANGO_FUNC( PangoContext *, (PangoFontMap *fontmap), pango_font_map_create_context )
WRAP_PANGO_FUNC( void, (PangoFontMap *fontmap, PangoFontFamily ***families, int *n_families), pango_font_map_list_families )
WRAP_PANGO_FUNC( PangoFont *, (PangoFontMap *fontmap, PangoContext *context, const PangoFontDescription *desc), pango_font_map_load_font )
WRAP_PANGO_FUNC( const char *, (PangoFontFamily *family), pango_font_family_get_name )
WRAP_PANGO_FUNC( gboolean, (PangoFontFamily *family), pango_font_family_is_monospace )
WRAP_PANGO_FUNC( PangoLayout *, (PangoContext *context), pango_layout_new )
WRAP_PANGO_FUNC( void, (PangoContext *context, PangoLanguage  *language), pango_context_set_language )
WRAP_PANGO_FUNC( PangoLanguage *, (const char *language), pango_language_from_string )
WRAP_PANGO_FUNC( PangoLanguage *, (PangoContext  *context), pango_context_get_language )
WRAP_PANGO_FUNC( void, (PangoContext *context, PangoDirection direction), pango_context_set_base_dir )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, gboolean auto_dir), pango_layout_set_auto_dir )
WRAP_PANGO_FUNC( PangoFontMetrics *, (PangoContext  *context, const PangoFontDescription   *desc,  PangoLanguage  *language), pango_context_get_metrics )
WRAP_PANGO_FUNC( PangoFontMap *, (PangoContext *context), pango_context_get_font_map )

WRAP_PANGO_FUNC( void, (PangoAttrList  *list, PangoAttribute *attr), pango_attr_list_change )
WRAP_PANGO_FUNC( void, (PangoAttrList  *list), pango_attr_list_unref )
WRAP_PANGO_FUNC( PangoAttrList *, (void), pango_attr_list_new )
WRAP_PANGO_FUNC( void, (PangoAttrList  *list, PangoAttribute *attr), pango_attr_list_insert )
WRAP_PANGO_FUNC( void, (PangoAttribute       *attr, const PangoAttrClass *klass), pango_attribute_init )
WRAP_PANGO_FUNC( PangoAttrType, (const gchar *name), pango_attr_type_register )
WRAP_PANGO_FUNC( PangoAttribute *, (gboolean strikethrough), pango_attr_strikethrough_new )
WRAP_PANGO_FUNC( PangoAttribute *, (PangoUnderline underline), pango_attr_underline_new )
WRAP_PANGO_FUNC( PangoAttribute *, (int size), pango_attr_size_new )
WRAP_PANGO_FUNC( PangoAttribute *, (PangoStyle  style), pango_attr_style_new )
WRAP_PANGO_FUNC( PangoAttribute *, (PangoWeight weight), pango_attr_weight_new )
WRAP_PANGO_FUNC( PangoAttribute *, (const char *family), pango_attr_family_new )
WRAP_PANGO_FUNC( PangoAttribute *, (int letter_spacing), pango_attr_letter_spacing_new )

WRAP_PANGO_FUNC( void, (PangoFontMetrics *metrics), pango_font_metrics_unref )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_ascent )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_descent )
WRAP_PANGO_FUNC( PangoFontMetrics *, (PangoFont        *font, PangoLanguage    *language), pango_font_get_metrics )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_strikethrough_position )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_strikethrough_thickness )
WRAP_PANGO_FUNC( void, (PangoFontDescription *desc, PangoWeight weight), pango_font_description_set_weight )
WRAP_PANGO_FUNC( void, (PangoFontDescription *desc, PangoStyle style), pango_font_description_set_style )
WRAP_PANGO_FUNC( void, (PangoFontDescription *desc, gint size), pango_font_description_set_size )
WRAP_PANGO_FUNC( PangoFontDescription *, (const char *str), pango_font_description_from_string )
WRAP_PANGO_FUNC( const char *, (const PangoFontDescription *desc), pango_font_description_get_family )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_underline_position )
WRAP_PANGO_FUNC( int, (PangoFontMetrics *metrics), pango_font_metrics_get_underline_thickness )
WRAP_PANGO_FUNC( void, (PangoFontDescription        *desc), pango_font_description_free )
WRAP_PANGO_FUNC( PangoFontDescription *, (PangoFont *font), pango_font_describe )
WRAP_PANGO_FUNC( char *, (const PangoFontDescription *desc), pango_font_description_to_string )
WRAP_PANGO_FUNC( const PangoFontDescription *, (PangoLayout* layout), pango_layout_get_font_description )

WRAP_PANGO_FUNC( void,  (PangoLayout *layout,const char *text, int length), pango_layout_set_text )
WRAP_PANGO_FUNC( const char *, (PangoLayout* layout), pango_layout_get_text )
WRAP_PANGO_FUNC( int, (PangoLayout    *layout), pango_layout_get_line_count )
WRAP_PANGO_FUNC( PangoLayoutLine *, (PangoLayout *layout, int line), pango_layout_get_line_readonly )
WRAP_PANGO_FUNC( void, (PangoLayout    *layout, PangoRectangle *ink_rect,PangoRectangle *logical_rect), pango_layout_get_pixel_extents )
WRAP_PANGO_FUNC( void, (PangoLayout    *layout, PangoRectangle *ink_rect,PangoRectangle *logical_rect), pango_layout_get_extents )
WRAP_PANGO_FUNC( void, (PangoLayout    *layout,PangoAttrList  *attrs), pango_layout_set_attributes )
WRAP_PANGO_FUNC( PangoAttrList *, (PangoLayout    *layout), pango_layout_get_attributes )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, const PangoFontDescription *desc), pango_layout_set_font_description )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, int  index_, PangoRectangle *pos), pango_layout_index_to_pos )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, int index_, gboolean  trailing, int *line, int  *x_pos), pango_layout_index_to_line_x )
WRAP_PANGO_FUNC( gboolean, (PangoLayout *layout, int x, int  y, int  *index_, int *trailing), pango_layout_xy_to_index )
WRAP_PANGO_FUNC( gboolean, (PangoLayout *layout), pango_layout_is_wrapped )
WRAP_PANGO_FUNC( PangoLayoutIter *, (PangoLayout     *layout), pango_layout_get_iter )
WRAP_PANGO_FUNC( void, (PangoLayoutIter *iter), pango_layout_iter_free )
WRAP_PANGO_FUNC( int, (PangoLayoutIter *iter), pango_layout_iter_get_index )
WRAP_PANGO_FUNC( PangoLayoutRun  *, (PangoLayoutIter *iter), pango_layout_iter_get_run )
WRAP_PANGO_FUNC( PangoLayoutRun  *, (PangoLayoutIter *iter), pango_layout_iter_get_run_readonly )
WRAP_PANGO_FUNC( PangoLayoutLine *, (PangoLayoutIter *iter), pango_layout_iter_get_line_readonly )
WRAP_PANGO_FUNC( gboolean, (PangoLayoutIter *iter), pango_layout_iter_next_line )
WRAP_PANGO_FUNC( void, (PangoLayoutIter *iter, PangoRectangle  *ink_rect, PangoRectangle  *logical_rect), pango_layout_iter_get_line_extents )
WRAP_PANGO_FUNC( void, (PangoLayoutIter *iter, PangoRectangle  *ink_rect, PangoRectangle  *logical_rect), pango_layout_iter_get_run_extents )
WRAP_PANGO_FUNC( int, (PangoLayoutIter *iter), pango_layout_iter_get_baseline )
WRAP_PANGO_FUNC( gboolean, (PangoLayoutIter *iter), pango_layout_iter_next_run )
WRAP_PANGO_FUNC( int, (PangoLayout *layout), pango_layout_get_height )
WRAP_PANGO_FUNC( int, (PangoLayout *layout), pango_layout_get_width )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, int width), pango_layout_set_width )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, int height), pango_layout_set_height )
WRAP_PANGO_FUNC( void, (PangoLayout   *layout, PangoEllipsizeMode  ellipsize), pango_layout_set_ellipsize )
WRAP_PANGO_FUNC( PangoEllipsizeMode, (PangoLayout *layout), pango_layout_get_ellipsize )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, PangoWrapMode wrap), pango_layout_set_wrap )
WRAP_PANGO_FUNC( void, (PangoLayout *layout, PangoAlignment alignment), pango_layout_set_alignment )
WRAP_PANGO_FUNC( PangoAttribute *, (const PangoRectangle *ink_rect, const PangoRectangle *logical_rect), pango_attr_shape_new )
WRAP_PANGO_FUNC( PangoWrapMode, (PangoLayout *layout ), pango_layout_get_wrap )
WRAP_PANGO_FUNC( gboolean, (PangoLayout *layout ), pango_layout_is_ellipsized )
WRAP_PANGO_FUNC( void, (PangoLayout *layout ), pango_layout_context_changed )
WRAP_PANGO_FUNC( void, (PangoRectangle *inclusive, PangoRectangle *nearest), pango_extents_to_pixels )
WRAP_PANGO_FUNC( void, (PangoLayoutLine *line,int start_index,int end_index,int **ranges,int *n_ranges), pango_layout_line_get_x_ranges )

WRAP_PANGO_FUNC( GType, (void), pango_fc_font_get_type )
WRAP_PANGO_FUNC( FT_Face, (PangoFcFont *font), pango_fc_font_lock_face )
WRAP_PANGO_FUNC( void, (PangoFcFont *font), pango_fc_font_unlock_face )


// defines to map the static symbol name back to the undecorated function name for easy use in shared code
#define pango_ft2_font_map_new s_pfnpango_ft2_font_map_new
#define pango_ft2_render_transformed s_pfnpango_ft2_render_transformed

#define pango_font_map_create_context s_pfnpango_font_map_create_context
#define pango_font_map_list_families s_pfnpango_font_map_list_families
#define pango_font_map_load_font s_pfnpango_font_map_load_font
#define pango_font_family_get_name s_pfnpango_font_family_get_name
#define pango_font_family_is_monospace s_pfnpango_font_family_is_monospace
#define pango_layout_new s_pfnpango_layout_new
#define pango_context_set_language s_pfnpango_context_set_language
#define pango_language_from_string s_pfnpango_language_from_string
#define pango_layout_set_width s_pfnpango_layout_set_width
#define pango_layout_set_height s_pfnpango_layout_set_height
#define pango_layout_set_ellipsize s_pfnpango_layout_set_ellipsize
#define pango_layout_get_ellipsize s_pfnpango_layout_get_ellipsize
#define pango_layout_set_wrap s_pfnpango_layout_set_wrap
#define pango_layout_set_alignment s_pfnpango_layout_set_alignment
#define pango_context_get_language s_pfnpango_context_get_language
#define pango_context_set_base_dir s_pfnpango_context_set_base_dir
#define pango_layout_set_auto_dir s_pfnpango_layout_set_auto_dir
#define pango_context_get_metrics s_pfnpango_context_get_metrics
#define pango_context_get_font_map s_pfnpango_context_get_font_map

#define pango_layout_set_text s_pfnpango_layout_set_text
#define pango_layout_get_text s_pfnpango_layout_get_text
#define pango_layout_get_pixel_extents s_pfnpango_layout_get_pixel_extents
#define pango_layout_get_extents s_pfnpango_layout_get_extents
#define pango_layout_set_attributes s_pfnpango_layout_set_attributes
#define pango_layout_get_attributes s_pfnpango_layout_get_attributes
#define pango_layout_set_font_description s_pfnpango_layout_set_font_description
#define pango_layout_index_to_pos s_pfnpango_layout_index_to_pos
#define pango_layout_index_to_line_x s_pfnpango_layout_index_to_line_x
#define pango_layout_xy_to_index s_pfnpango_layout_xy_to_index
#define pango_layout_is_wrapped s_pfnpango_layout_is_wrapped
#define pango_layout_get_iter s_pfnpango_layout_get_iter
#define pango_layout_iter_free s_pfnpango_layout_iter_free
#define pango_layout_iter_get_index s_pfnpango_layout_iter_get_index
#define pango_layout_iter_get_run s_pfnpango_layout_iter_get_run
#define pango_layout_iter_get_run_readonly s_pfnpango_layout_iter_get_run_readonly
#define pango_layout_iter_get_line_readonly s_pfnpango_layout_iter_get_line_readonly
#define pango_layout_iter_next_line s_pfnpango_layout_iter_next_line
#define pango_layout_iter_get_line_extents s_pfnpango_layout_iter_get_line_extents
#define pango_layout_iter_get_run_extents s_pfnpango_layout_iter_get_run_extents
#define pango_layout_iter_get_baseline s_pfnpango_layout_iter_get_baseline
#define pango_layout_iter_next_run s_pfnpango_layout_iter_next_run
#define pango_layout_get_height s_pfnpango_layout_get_height
#define pango_layout_get_width s_pfnpango_layout_get_width
#define pango_layout_get_line_count s_pfnpango_layout_get_line_count
#define pango_layout_get_line_readonly s_pfnpango_layout_get_line_readonly
#define pango_extents_to_pixels s_pfnpango_extents_to_pixels
#define pango_layout_line_get_x_ranges s_pfnpango_layout_line_get_x_ranges

#define pango_font_metrics_unref  s_pfnpango_font_metrics_unref
#define pango_font_metrics_get_descent s_pfnpango_font_metrics_get_descent
#define pango_font_metrics_get_ascent s_pfnpango_font_metrics_get_ascent
#define pango_font_get_metrics s_pfnpango_font_get_metrics
#define pango_font_metrics_get_strikethrough_position s_pfnpango_font_metrics_get_strikethrough_position
#define pango_font_metrics_get_strikethrough_thickness s_pfnpango_font_metrics_get_strikethrough_thickness
#define pango_font_description_set_weight s_pfnpango_font_description_set_weight
#define pango_font_description_set_style s_pfnpango_font_description_set_style
#define pango_font_description_set_size s_pfnpango_font_description_set_size
#define pango_font_description_from_string s_pfnpango_font_description_from_string
#define pango_font_description_get_family s_pfnpango_font_description_get_family
#define pango_font_metrics_get_underline_position s_pfnpango_font_metrics_get_underline_position
#define pango_font_metrics_get_underline_thickness s_pfnpango_font_metrics_get_underline_thickness
#define pango_font_description_free s_pfnpango_font_description_free

#define pango_attr_list_change s_pfnpango_attr_list_change
#define pango_attr_list_unref s_pfnpango_attr_list_unref
#define pango_attr_list_insert s_pfnpango_attr_list_insert
#define pango_attr_list_new s_pfnpango_attr_list_new
#define pango_attribute_init s_pfnpango_attribute_init
#define pango_attr_type_register s_pfnpango_attr_type_register
#define pango_attr_strikethrough_new s_pfnpango_attr_strikethrough_new
#define pango_attr_underline_new s_pfnpango_attr_underline_new
#define pango_attr_weight_new s_pfnpango_attr_weight_new
#define pango_attr_style_new s_pfnpango_attr_style_new
#define pango_attr_size_new s_pfnpango_attr_size_new
#define pango_attr_family_new s_pfnpango_attr_family_new
#define pango_attr_letter_spacing_new s_pfnpango_attr_letter_spacing_new
#define pango_attr_shape_new s_pfnpango_attr_shape_new
#define pango_layout_get_wrap s_pfnpango_layout_get_wrap
#define pango_layout_is_ellipsized s_pfnpango_layout_is_ellipsized
#define pango_layout_context_changed s_pfnpango_layout_context_changed

#define pango_fc_font_get_type s_pfnpango_fc_font_get_type
#define pango_fc_font_lock_face s_pfnpango_fc_font_lock_face
#define pango_fc_font_unlock_face s_pfnpango_fc_font_unlock_face

#define pango_font_describe s_pfnpango_font_describe
#define pango_font_description_to_string s_pfnpango_font_description_to_string
#define pango_layout_get_font_description s_pfnpango_layout_get_font_description

//-----------------------------------------------------------------------------
// Purpose: release the shared libraries for our custom pango
//-----------------------------------------------------------------------------
void FreePangoFuncPointers()
{
	if ( s_hPangoModule )
	{
		dlclose( s_hPangoModule );
		s_hPangoModule = 0;
	}

	if ( s_hPangoFT2Module )
	{
		dlclose( s_hPangoFT2Module );
		s_hPangoFT2Module = 0;
	}

	fontconfig_ft2_new_face_substitute = NULL;
	pango_ft2_new_face_substitute = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: dlopen our own custom pango and suck in all the function pointers we need
//-----------------------------------------------------------------------------
static void *CheckDeepLoadModule( const char *pPath )
{
    // We deep-bind immediately to keep all symbol resolution
    // for pango confined to the pango binaries.
#ifdef PANORAMA_USE_S1WRAPPER
	// Remove DEEPBind when loading module to fix a crash in free() - memory allocated with tcmalloc
	// but using regular libc free (instead of tcmalloc)
	// (https://github.com/gperftools/gperftools/issues/230)
	void *pModule = dlopen( pPath, RTLD_NOW );
#else
	void *pModule = dlopen( pPath, RTLD_NOW | RTLD_DEEPBIND );
#endif
    if ( !pModule )
    {
        const char *pError = dlerror();
        if ( pError )
        {
            Plat_FatalError( "Unable to load '%s' (error info '%s'), your game install may be corrupted or you may have a system conflict\n", pPath, pError );
        }
        else
        {
            Plat_FatalError( "Unable to load '%s', your game install may be corrupted or you may have a system conflict\n", pPath );
        }
    }

    return pModule;
}

void SetupPangoFuncPointers()
{
	// free our existing handles if we have them
	if ( s_hPangoModule || s_hPangoFT2Module )
		FreePangoFuncPointers();

	s_hPangoFT2Module = CheckDeepLoadModule( "libpangoft2-1.0.so" );
    s_hPangoModule = CheckDeepLoadModule( "libpango-1.0.so" );
    void *s_hFreeType = s_hPangoFT2Module; // we statically link all our dependencies together so we only need to reach into these 2 .so for our symbols
    void *s_hFontConfig = s_hPangoFT2Module;
    void *s_hGObject = s_hPangoModule;

    FcConfigAppFontAddFile = (PFNFcConfigAppFontAddFile)dlsym( s_hFontConfig, "FcConfigAppFontAddFile" );
    Assert( FcConfigAppFontAddFile );
	FcConfigAppFontAddDir = (PFNFcConfigAppFontAddDir)dlsym( s_hFontConfig, "FcConfigAppFontAddDir" );
	Assert( FcConfigAppFontAddDir );
	FT_New_Memory_Face = (PFNFT_New_Memory_Face)dlsym( s_hFreeType, "FT_New_Memory_Face" );
    Assert( FT_New_Memory_Face );
    FT_Load_Glyph = (PFNFT_Load_Glyph)dlsym( s_hFreeType, "FT_Load_Glyph" );
    Assert( FT_Load_Glyph );
    FT_Get_Char_Index = (PFNFT_Get_Char_Index)dlsym( s_hFreeType, "FT_Get_Char_Index" );
    Assert( FT_Get_Char_Index );
    FT_Get_Kerning = (PFNFT_Get_Kerning)dlsym( s_hFreeType, "FT_Get_Kerning" );
    Assert( FT_Get_Kerning );
    fontconfig_ft2_new_face_substitute = (PFNfontconfig_ft2_new_face_substitute)dlsym( s_hFontConfig, "fontconfig_ft2_new_face_substitute" );
    Assert( fontconfig_ft2_new_face_substitute );
    pango_ft2_new_face_substitute = (PFNfontconfig_ft2_new_face_substitute)dlsym( s_hPangoFT2Module, "pango_ft2_new_face_substitute" );
    Assert( pango_ft2_new_face_substitute );
    g_object_unref = (PFNg_object_unref)dlsym( s_hGObject, "g_object_unref" );
    Assert( g_object_unref );
    g_free = (PFNg_free)dlsym( s_hGObject, "g_free" );
    Assert( g_free );
    g_type_check_instance_is_a = (PFNg_type_check_instance_is_a)dlsym( s_hGObject, "g_type_check_instance_is_a" );
    Assert( g_type_check_instance_is_a );
    g_type_check_instance_cast = (PFNg_type_check_instance_cast)dlsym( s_hGObject, "g_type_check_instance_cast" );
    Assert( g_type_check_instance_cast );

    LOAD_PANGO_FT2_PROC( pango_ft2_render_transformed );
    LOAD_PANGO_FT2_PROC( pango_ft2_font_map_new );
    
    LOAD_PANGO_PROC( pango_font_map_create_context );
    LOAD_PANGO_PROC( pango_font_map_list_families );
    LOAD_PANGO_PROC( pango_font_map_load_font );
    LOAD_PANGO_PROC( pango_font_family_get_name );
    LOAD_PANGO_PROC( pango_font_family_is_monospace );
    LOAD_PANGO_PROC( pango_layout_new );
    LOAD_PANGO_PROC( pango_context_set_language );
    LOAD_PANGO_PROC( pango_language_from_string );
    LOAD_PANGO_PROC( pango_layout_set_width );
    LOAD_PANGO_PROC( pango_layout_set_height );
    LOAD_PANGO_PROC( pango_layout_set_ellipsize );
    LOAD_PANGO_PROC( pango_layout_get_ellipsize );
    LOAD_PANGO_PROC( pango_layout_set_wrap );
    LOAD_PANGO_PROC( pango_layout_set_alignment );
    LOAD_PANGO_PROC( pango_context_get_language );
    LOAD_PANGO_PROC( pango_context_set_base_dir );
	LOAD_PANGO_PROC( pango_layout_set_auto_dir );
	LOAD_PANGO_PROC( pango_context_get_metrics );
    LOAD_PANGO_PROC( pango_context_get_font_map );

    LOAD_PANGO_PROC( pango_layout_set_text );
    LOAD_PANGO_PROC( pango_layout_get_text );
    LOAD_PANGO_PROC( pango_layout_get_pixel_extents );
    LOAD_PANGO_PROC( pango_layout_get_extents );
    LOAD_PANGO_PROC( pango_layout_set_attributes );
    LOAD_PANGO_PROC( pango_layout_get_attributes );
    LOAD_PANGO_PROC( pango_layout_set_font_description );
    LOAD_PANGO_PROC( pango_layout_index_to_pos );
    LOAD_PANGO_PROC( pango_layout_index_to_line_x );
    LOAD_PANGO_PROC( pango_layout_xy_to_index );
    LOAD_PANGO_PROC( pango_layout_is_wrapped );
    LOAD_PANGO_PROC( pango_layout_get_iter );
    LOAD_PANGO_PROC( pango_layout_iter_free );
    LOAD_PANGO_PROC( pango_layout_iter_get_index );
    LOAD_PANGO_PROC( pango_layout_iter_get_run );
    LOAD_PANGO_PROC( pango_layout_iter_get_run_readonly );
    LOAD_PANGO_PROC( pango_layout_iter_get_line_readonly );
    LOAD_PANGO_PROC( pango_layout_iter_next_line );
    LOAD_PANGO_PROC( pango_layout_iter_get_line_extents );
    LOAD_PANGO_PROC( pango_layout_iter_get_run_extents );
    LOAD_PANGO_PROC( pango_layout_iter_get_baseline );
    LOAD_PANGO_PROC( pango_layout_iter_next_run );
    LOAD_PANGO_PROC( pango_layout_get_height );
    LOAD_PANGO_PROC( pango_layout_get_width );
    LOAD_PANGO_PROC( pango_layout_get_line_count );
    LOAD_PANGO_PROC( pango_layout_get_line_readonly );
    LOAD_PANGO_PROC( pango_extents_to_pixels );
    LOAD_PANGO_PROC( pango_layout_line_get_x_ranges );

    LOAD_PANGO_PROC( pango_font_metrics_unref );
    LOAD_PANGO_PROC( pango_font_metrics_get_descent );
    LOAD_PANGO_PROC( pango_font_metrics_get_ascent );
    LOAD_PANGO_PROC( pango_font_get_metrics );
    LOAD_PANGO_PROC( pango_font_metrics_get_strikethrough_position );
    LOAD_PANGO_PROC( pango_font_metrics_get_strikethrough_thickness );
    LOAD_PANGO_PROC( pango_font_description_set_weight );
    LOAD_PANGO_PROC( pango_font_description_set_style );
    LOAD_PANGO_PROC( pango_font_description_set_size );
    LOAD_PANGO_PROC( pango_font_description_from_string );
    LOAD_PANGO_PROC( pango_font_description_get_family );
    LOAD_PANGO_PROC( pango_font_metrics_get_underline_position );
    LOAD_PANGO_PROC( pango_font_metrics_get_underline_thickness );
    LOAD_PANGO_PROC( pango_font_description_free );

    LOAD_PANGO_PROC( pango_attr_list_change );
    LOAD_PANGO_PROC( pango_attr_list_unref );
    LOAD_PANGO_PROC( pango_attr_list_insert );
    LOAD_PANGO_PROC( pango_attr_list_new );
    LOAD_PANGO_PROC( pango_attribute_init );
    LOAD_PANGO_PROC( pango_attr_type_register );
    LOAD_PANGO_PROC( pango_attr_strikethrough_new );
    LOAD_PANGO_PROC( pango_attr_underline_new );
    LOAD_PANGO_PROC( pango_attr_weight_new );
    LOAD_PANGO_PROC( pango_attr_style_new );
    LOAD_PANGO_PROC( pango_attr_size_new );
    LOAD_PANGO_PROC( pango_attr_family_new );
    LOAD_PANGO_PROC( pango_attr_letter_spacing_new );
    LOAD_PANGO_PROC( pango_attr_shape_new );
    LOAD_PANGO_PROC( pango_layout_get_wrap );
    LOAD_PANGO_PROC( pango_layout_is_ellipsized );
    LOAD_PANGO_PROC( pango_layout_context_changed );

    LOAD_PANGO_FT2_PROC( pango_fc_font_get_type );
    LOAD_PANGO_FT2_PROC( pango_fc_font_lock_face );
    LOAD_PANGO_FT2_PROC( pango_fc_font_unlock_face );

	LOAD_PANGO_PROC( pango_font_describe );
	LOAD_PANGO_PROC( pango_font_description_to_string );
	LOAD_PANGO_PROC( pango_layout_get_font_description );

}
#endif

//-----------------------------------------------------------------------------
// Purpose: startup, make sure pango is configured
//-----------------------------------------------------------------------------
void CUITextLayoutPango::Initialize( const void * pZeroFillBuffer, int nZeroFillBufferSize )
{
	AUTO_LOCK( s_GlobalMutex );
	++s_nInitializeCount;

	s_pZeroFillBuffer = pZeroFillBuffer;
	s_nZeroFillBufferSize = nZeroFillBufferSize;

	if ( s_nInitializeCount == 1 )
	{
#if defined( SOURCE2_PANORAMA )
		char szAbsolutePathScratch[ 1024 ];
		g_pFullFileSystem->RelativePathToFullPath( "panorama/fonts/fonts.conf", "GAME", szAbsolutePathScratch, sizeof( szAbsolutePathScratch ) );
		V_StripFilename( szAbsolutePathScratch );
		SDL_setenv( "FONTCONFIG_PATH", szAbsolutePathScratch, 1 );
		V_MakeAbsolutePath( szAbsolutePathScratch, ARRAYSIZE(szAbsolutePathScratch),  ".", NULL, false );
		SDL_setenv( "PANGO_SYSCONFDIR", szAbsolutePathScratch, 1 );
		SDL_setenv( "PANGO_LIBDIR", szAbsolutePathScratch, 1 );
#else
		CUtlString sPath = UIEngine()->GetApplicationInstallPath();
		if ( IsOSX() )
		{
			sPath += "/bin/panorama/etc/fonts";
		}
		else
		{
			sPath += "/../bin/panorama/etc/fonts";

		}
		V_MakeAbsolutePath( sPath.Access(), sPath.Length() + 1, sPath.String() );

		SDL_setenv( "FONTCONFIG_PATH", sPath.String(), 1 );

#endif // SOURCE2_PANORAMA

#ifdef LINUX
		SetupPangoFuncPointers();
#endif

		CUIFontLoaderLinux::GetInstance().Initialize();
	}

	if ( !s_pFontmap )
	{
		s_pFontmap = pango_ft2_font_map_new();
        if ( !s_pFontmap )
        {
            Plat_FatalError( "Unable to create Pango font map\n" );
        }
        
#if 0
		pango_ft2_font_map_set_resolution( PANGO_FT2_FONT_MAP( s_pFontmap ), 96, 96 );
		// pango_ft2_font_map_set_default_substitute( PANGO_FT2_FONT_MAP( s_pFontmap ), SubstituteFunc, NULL, NULL );
#endif
	}

	if ( !s_pContext )
	{
		// Setup context.
		// TODO: some of this is i18n specific
		s_pContext = pango_font_map_create_context( s_pFontmap );
        if ( !s_pContext )
        {
            Plat_FatalError( "Unable to create Pango context\n" );
        }

		static ConVarRef cl_language( "cl_language" );
		const char *pLanguageCode = GetLanguageICUName( PchLanguageToELanguage( cl_language.GetString() ) );

		pango_context_set_language( s_pContext, pango_language_from_string( pLanguageCode ) );
		pango_context_set_base_dir( s_pContext, PANGO_DIRECTION_LTR );

		if( V_stricmp( pLanguageCode, "th_TH" ) == 0 )
		{
			// Change the word wrap mode used for Thai because Thai doesn't use spaces between words.
			// PANGO_WRAP_WORD_CHAR wraps lines at word boundaries, but falls back to character boundaries 
			// if there is not enough space for a full word
			s_eWordWrapMode = PANGO_WRAP_WORD_CHAR;
		}

	}
}


//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CUITextLayoutPango::Shutdown()
{
	AUTO_LOCK( s_GlobalMutex );	
	--s_nInitializeCount;

	if ( s_nInitializeCount == 0 )
	{
		if ( s_pContext != NULL )
		{
			g_object_unref( s_pContext );
			s_pContext = NULL;
		}

		if ( s_pFontmap != NULL )
		{
			g_object_unref( s_pFontmap );
			s_pFontmap = NULL;
		}

#if defined( LINUX)
		FreePangoFuncPointers();
#endif
		CUIFontLoaderLinux::GetInstance().Shutdown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the font list from the current pango context
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::GetFontList( CUtlSortVector< CUtlString > *pList )
{
	if ( !s_pContext )
	{
		return false;
	}
	
	PangoFontMap *pFontMap = pango_context_get_font_map( s_pContext );
	if ( !pFontMap )
	{
		return false;
	}
	
	pList->RemoveAll();

	PangoFontFamily **pFontFamilies;
	int cFontFamilies;
	pango_font_map_list_families( pFontMap, &pFontFamilies, &cFontFamilies );
#ifdef SOURCE2_PANORAMA
	for ( int i = 0; i < cFontFamilies; i++ )
	{
		pList->InsertNoSort( pango_font_family_get_name( pFontFamilies[i] ) );
	}
	pList->RedoSort();
#else
	for ( int i = 0; i < cFontFamilies; i++ )
	{
		pList->Insert( pango_font_family_get_name( pFontFamilies[i] ) );
	}
#endif

	g_free( pFontFamilies );


	return true;
}

void CUITextLayoutPango::SetDebugFontSelection( bool bEnable )
{
	s_bDebugFontSelection = bEnable;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUITextLayoutPango::CUITextLayoutPango()
{
	//Msg( "CUITextLayoutPango count: %llu\n", s_ulLayoutCount );
	m_pLayout = NULL;
	m_bEllipsisNoWrap = false;
    m_iWidth = -1;
    m_iFirstLineHeightAdjust = 0;
	m_flMaxHeight = 0;
	m_flLineSpacing = 0;
	m_flLineSpacingAscent = 0;
	m_nLetterSpacing = 0;
	m_eFontStyle = k_EFontStyleUnset;
    m_pPangoFcFont = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUITextLayoutPango::~CUITextLayoutPango()
{
	Clear();

	//Msg( "~CUITextLayoutPango count: %llu\n", s_ulLayoutCount );
}

void CUITextLayoutPango::Clear()
{
  	AUTO_LOCK( s_PangoMutex );
	g_clear_object( &m_pPangoFcFont );
	g_clear_object( &m_pLayout );
}


bool CUITextLayoutPango::Resize( float flMaxWidth, float flMaxHeight )
{
	if( !m_pLayout )
	{
		return false;
	}
	
	bool bWrap = (m_iWidth != -1);

	if( flMaxWidth != k_flFloatNotSet && bWrap )
	{
		int iNewWidth = (int)(Min( k_flMaxWidthOrHeight, ceilf( flMaxWidth ) ));
		if( iNewWidth != m_iWidth )
		{
			AUTO_LOCK( s_PangoMutex );

			m_iWidth = iNewWidth;
			pango_layout_set_width( m_pLayout, m_iWidth * PANGO_SCALE );
		}
	}

	int iNewHeight = ceilf( flMaxHeight );

	if( iNewHeight != (int)ceilf( m_flMaxHeight ) )
	{
		m_flMaxHeight = flMaxHeight;

		if( flMaxHeight != k_flFloatNotSet && !m_bEllipsisNoWrap )
		{
			AUTO_LOCK( s_PangoMutex );
			pango_layout_set_height( m_pLayout, iNewHeight * PANGO_SCALE );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics )
{
	VPROF_BUDGET( "CUITextLayoutPango::BInitialize", VPROF_BUDGETGROUP_TENFOOT );

	if ( pLayoutMetrics )
	{
		memset( pLayoutMetrics, 0, sizeof(*pLayoutMetrics) );
	}

	CStrAutoEncodeSrc2 strEncode( L"" );
	const char *pchText;

	switch( eTextEncoding )
	{
	case k_EPanoramaTextEncodingUTF8:
		pchText = (const char *)pRawText;
		break;
	case k_EPanoramaTextEncodingUChar16:
		strEncode.Set( (const uchar16*)pRawText );
		pchText = strEncode.ToUTF8();
		break;
	case k_EPanoramaTextEncodingUChar32:
		strEncode.Set( (const uchar32*)pRawText );
		pchText = strEncode.ToUTF8();
		break;
	default:
		AssertMsg( false, "Unknown text encoding" );
		return false;
	}

	AUTO_LOCK( s_PangoMutex );

	// Setup layout properties
	// TODO: Pango supports width restriction but not height restriction. If height restricted layout is needed, it will
	// need to be iteratively achieved.
	m_pLayout = pango_layout_new( s_pContext );
	pango_layout_set_auto_dir( m_pLayout, FALSE );
	pango_layout_set_text( m_pLayout, pchText, V_strlen( pchText ) );
	m_nTextChars = cTextChars;
	m_iWidth = -1;

	m_eFontStyle = pParams->m_style;

	// If wrapping and ellipsis are both disabled, don't set a width
	// From the pango docs: "To turn off wrapping, set the width to -1"
	// pango_layout_set_width - "Sets the width to which the lines of the PangoLayout should wrap or ellipsized. The default value is -1: no width set"
	m_flMaxHeight = pParams->m_flMaxHeight;
	if ( pParams->m_flMaxWidth != k_flFloatNotSet && (pParams->m_bWrap || pParams->m_bEllipsis) )
	{
		m_iWidth = (int)( Min( k_flMaxWidthOrHeight, ceilf( pParams->m_flMaxWidth ) ) );
		// pango is a pixel based layout engine so we always want to push up
		// to the next pixel boundary for size. This fixes rounding errors causing
		// input widths being slightly less than the pixel size we need, then causing elippses
		pango_layout_set_width( m_pLayout, m_iWidth * PANGO_SCALE );
	}
	if ( pParams->m_flMaxHeight != k_flFloatNotSet )
		pango_layout_set_height( m_pLayout, ceilf( pParams->m_flMaxHeight ) * PANGO_SCALE );
	if ( pParams->m_bEllipsis )
		pango_layout_set_ellipsize( m_pLayout, PANGO_ELLIPSIZE_END );
	else
		pango_layout_set_ellipsize( m_pLayout, PANGO_ELLIPSIZE_NONE );

	if ( pParams->m_bWrap )
	{
		// Word wrap mode is usually PANGO_WRAP_WORD, but is set to PANGO_WRAP_WORD_CHAR for Thai
		// because Thai doesn't use spaces between words.
		pango_layout_set_wrap( m_pLayout, s_eWordWrapMode );
	}
	// Do we want a single line with no wrapping but ellispsis?
	else if ( pParams->m_bEllipsis )
	{
		// It's not possible to ellipsize without a width, so instead keep 
		// word wrapping on and limit the height. According to the pango source,
		// a 0 height will always lay out one line.
		pango_layout_set_height( m_pLayout, 0 );

		// Set word wrapping to wrap at chars instead of words
		pango_layout_set_wrap( m_pLayout, PANGO_WRAP_CHAR );

		// Later we'll want to know about this in GetRequiredSize()
		m_bEllipsisNoWrap = true;
	}

	m_eAlign = pParams->m_align;
	if ( pParams->m_align != k_ETextAlignUnset )
		pango_layout_set_alignment( m_pLayout, GetPangoAlignment( pParams->m_align ) );

	// Setup initial font description
	// Always fallback to arial for missing glyphs in the main font 
	const char *pchLanguageFallback = "Arial";

	CFmtStr strFontList( "%s,%s", pParams->m_pchFontName, pchLanguageFallback );

	PangoFontDescription *pFontDescription = pango_font_description_from_string( strFontList );
	pango_font_description_set_size( pFontDescription, pParams->m_flSize * PANGO_SCALE );
	if ( pParams->m_style != k_EFontStyleUnset )
		pango_font_description_set_style( pFontDescription, GetPangoFontStyle( pParams->m_style ) );
	if ( pParams->m_weight != k_EFontWeightUnset )
		pango_font_description_set_weight( pFontDescription, GetPangoFontWeight( pParams->m_weight ) );
	pango_layout_set_font_description( m_pLayout, pFontDescription );

	m_nLetterSpacing = pParams->m_nLetterSpacing;
	if ( pParams->m_nLetterSpacing != 0 )
	{
		PangoAttribute *pAttr = pango_attr_letter_spacing_new( pParams->m_nLetterSpacing * PANGO_SCALE );
		pAttr->start_index = 0;
		pAttr->end_index = G_MAXUINT;

		SetAttribute( pAttr );
	}

 	m_iFirstLineHeightAdjust = 0;
    // Pango's spacing is between lines whereas Panorama wants to define
    // the full line height from top to baseline.
    // We translate between the two by subtracting out the font size
    // from the line height, but this is imprecise and can lead
    // to layout issues, particularly since many Panorama layouts are
    // pixel-based and so very fragile.

	{
		//VPROF_BUDGET( "CUITextLayoutPango::pango_font_map_load_font", VPROF_BUDGETGROUP_TENFOOT );

		PangoFont *pPangoFont = pango_font_map_load_font( s_pFontmap, s_pContext, pFontDescription );
		if( pPangoFont )
		{
			m_pPangoFcFont = PANGO_FC_FONT( pPangoFont );
		}
	}

	bool bHaveLineHeight = false;

	if ( pParams->m_flLineHeight != k_flFloatNotSet )
	{
		PangoFontMetrics *pFontMetrics = pango_context_get_metrics( s_pContext, pFontDescription, pango_context_get_language( s_pContext ) );
		int nAscent = pango_font_metrics_get_ascent( pFontMetrics );
		int nDescent = pango_font_metrics_get_descent( pFontMetrics );
	
		// use dwrite equivalent values for spacing, 80% of the height from the baseline to the top of the cell, and 20% below
		m_flLineSpacing = (float)( 0.2 * ( pParams->m_flLineHeight * PANGO_SCALE ) - nDescent );
		m_flLineSpacingAscent = pParams->m_flLineHeight * 0.8 * PANGO_SCALE - nAscent;

		// BUGBUG: ideally we would use pango_layout_set_spacing here but it causes truncated
		// text when used, and also doesn't implement the 80% algorithm above.

		// The line-height attribute is unfortunately sometimes used
		// to try and bottom-align different pieces of text.  It's
		// also usually given in pixels, so it almost never does exactly what
		// is intended given the variations in font size on different
		// platforms.  Eventually line-height should probably go away
		// but it's used in some non-trivial ways now so simulate
		// some line offsetting.  This will never be pixel-exact
		// between platforms but hopefully is close.
		m_iFirstLineHeightAdjust = ceil( m_flLineSpacing / PANGO_SCALE ) - 1;
		if ( m_iFirstLineHeightAdjust < 0 )
		{
			m_iFirstLineHeightAdjust = 0;
		}

		// if we have manually set line spacing, let us just take over the size to draw so it doesn't truncate
		if ( m_flLineSpacing + m_flLineSpacingAscent < 0 )
			pango_layout_set_height( m_pLayout, INT_MAX );

		if ( pLayoutMetrics )
		{
			pLayoutMetrics->m_flLineHeight = pParams->m_flLineHeight;
			pLayoutMetrics->m_flAscent = (float)nAscent / PANGO_SCALE;
			pLayoutMetrics->m_flDescent = (float)nDescent / PANGO_SCALE;
		}

		pango_font_metrics_unref( pFontMetrics );
		bHaveLineHeight = true;
	}
	else
	{
		m_flLineSpacingAscent = 0;
		m_flLineSpacing = 0;

		if ( pLayoutMetrics )
		{
			PangoFontMetrics *pFontMetrics = pango_context_get_metrics( s_pContext, pFontDescription, pango_context_get_language( s_pContext ) );
			int nAscent = pango_font_metrics_get_ascent( pFontMetrics );
			int nDescent = pango_font_metrics_get_descent( pFontMetrics );
			pLayoutMetrics->m_flAscent = (float)nAscent / PANGO_SCALE;
			pLayoutMetrics->m_flDescent = (float)nDescent / PANGO_SCALE;

			// Pango doesn't seem to expose a line height metric for the layout.  If we have
			// some text try and guess it from a character bbox, otherwise just
			// use the ascent and descent (which is wrong since it doesn't have leading).
			if ( pchText[0] )
			{
				PangoRectangle pos;
				pango_layout_index_to_pos( m_pLayout, 0, &pos );
				pLayoutMetrics->m_flLineHeight = (float)( pos.y + pos.height ) / PANGO_SCALE;
				bHaveLineHeight = true;
			}

			pango_font_metrics_unref( pFontMetrics );
		}
	}

	if ( pLayoutMetrics )
	{
		if ( m_pPangoFcFont )
		{
			FT_Face face = pango_fc_font_lock_face( m_pPangoFcFont );
			if ( face )
			{
				pLayoutMetrics->m_bHaveCharMetrics = true;
				pLayoutMetrics->m_bFixedCharWidth = FT_IS_FIXED_WIDTH( face ) != 0;
				pLayoutMetrics->m_flMaxCharWidth = (float)face->size->metrics.max_advance / FREETYPE_SCALE;

				if ( !bHaveLineHeight )
				{
					pLayoutMetrics->m_flLineHeight = (float)face->size->metrics.height / FREETYPE_SCALE;
					bHaveLineHeight = true;
				}

				pango_fc_font_unlock_face( m_pPangoFcFont );
			}
		}

		if ( !bHaveLineHeight )
		{
			pLayoutMetrics->m_flLineHeight = pLayoutMetrics->m_flAscent + pLayoutMetrics->m_flDescent;
		}
    }
    
	pango_font_description_free( pFontDescription );

#if 0 // Debugging
	PangoFontMap *pFontMap = pango_context_get_font_map( s_pContext );
	PangoFontFamily **pFontFamilies;
	int cFontFamilies;
	pango_font_map_list_families( pFontMap, &pFontFamilies, &cFontFamilies );
	for ( int i = 0; i < cFontFamilies; i++ )
	{
		Msg( "%d: %s %d\n", i, pango_font_family_get_name( pFontFamilies[i] ), pango_font_family_is_monospace( pFontFamilies[i] ) );
	}
	g_free( pFontFamilies );
#endif

	return true;
}


PangoStyle CUITextLayoutPango::GetPangoFontStyle( EFontStyle style )
{
	switch ( style )
	{
	default:
	case k_EFontStyleUnset:
		Assert( false );
	case k_EFontStyleNormal:
		return PANGO_STYLE_NORMAL;
	case k_EFontStyleItalic:
		return PANGO_STYLE_ITALIC;
	}
}


PangoAlignment CUITextLayoutPango::GetPangoAlignment( ETextAlign align )
{
	switch ( align )
	{
	default:
	case k_ETextAlignUnset:
		Assert( false );
	case k_ETextAlignLeft:
		return PANGO_ALIGN_LEFT;
	case k_ETextAlignCenter:
		return PANGO_ALIGN_CENTER;
	case k_ETextAlignRight:
		return PANGO_ALIGN_RIGHT;
	}
}


PangoWeight CUITextLayoutPango::GetPangoFontWeight( EFontWeight weight )
{
	switch ( weight )
	{
	default:
	case k_EFontWeightUnset:
		Assert( false );
	case k_EFontWeightNormal:
		return PANGO_WEIGHT_NORMAL;
	case k_EFontWeightMedium:
		return PANGO_WEIGHT_MEDIUM;
	case k_EFontWeightBold:
		return PANGO_WEIGHT_BOLD;
	case k_EFontWeightBlack:
		return PANGO_WEIGHT_HEAVY;
	case k_EFontWeightThin:
		return PANGO_WEIGHT_THIN;
	case k_EFontWeightLight:
		return PANGO_WEIGHT_LIGHT;
	case k_EFontWeightSemiBold:
		return PANGO_WEIGHT_SEMIBOLD;
	}
}


//-----------------------------------------------------------------------------
// Purpose: finds the index for a render format that starts at a certain position and is a color change
//-----------------------------------------------------------------------------
int CUITextLayoutPango::FindNextTextRangeWithColorAndInRange( const UITextFormatProperties_t *pFormatProps, int cFormatProps, uint iStartIndex, uint iEndIndex, int iCurrent )
{
	iCurrent++;
	for ( int i = iCurrent; i < cFormatProps; i++ )
	{
		const UITextFormatProperties_t *pRangeColor = pFormatProps + i;
		if ( iStartIndex >= pRangeColor->m_unCharStartIndex && iEndIndex <= pRangeColor->m_unCharEndIndex )
			return pRangeColor->m_iColorIndex;
	}
	
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: create opacity masks for our string
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::BDraw( CUtlVector<UITextOpacityMaskDataRange_t> &drawRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, IUITextTextureStorage *pStorage, float flHeight, void *pRenderContext )
{
	VPROF_BUDGET( "CUITextLayoutPango::Draw", VPROF_BUDGETGROUP_TENFOOT );

	REFERENCE( pRenderContext );

	int nMaxStringSizePixels = MAX( 1024, (pStorage->GetMaximumTextureWidth()*1.1) )*PANGO_SCALE;
	CUtlVector<TextLayoutPangoBitmapRange_t> vecBitmaps;
	if( BDrawBitmaps( vecBitmaps, pFormatProps, cFormatProps, nMaxStringSizePixels ) )
	{
		int rangeCount = vecBitmaps.Count();
		if( rangeCount )
		{
			drawRanges.EnsureCapacity( rangeCount );

			for( int i = 0; i < rangeCount; ++i )
			{
				UITextOpacityMaskDataRange_t &drawRange = drawRanges[drawRanges.AddToTail()];
				UpdateTextureForRun( drawRange, vecBitmaps[i], pStorage );
			}
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: create opacity masks for our string
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::BDrawBitmaps( CUtlVector<TextLayoutPangoBitmapRange_t> &bitmapRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, int nMaxStringSizePixels )
{
	VPROF_BUDGET( "CUITextLayoutPango::DrawBitmaps", VPROF_BUDGETGROUP_TENFOOT );

	AUTO_LOCK( s_PangoMutex );

	if ( m_pLayout == NULL )
		return false;

	// For each line...
	bool bDrawText = false;
	int yPULineTop = 0;
	int iLine = 0;
	int iYAdjust = m_iFirstLineHeightAdjust;
	
	PangoLayoutIter *pIter;
	{
		VPROF_BUDGET( "CUITextLayoutPango::Draw - pango_layout_get_iter", VPROF_BUDGETGROUP_TENFOOT );
		pIter = pango_layout_get_iter( m_pLayout );
	}
	PangoLayoutLine *pLayoutLine;

	if( s_bDebugFontSelection )
	{
		char *pchFontDesc = pango_font_description_to_string( pango_layout_get_font_description( m_pLayout ) );
		Msg( "Text \"%s\" requested with preferred list of fonts \"%s\"\n", pango_layout_get_text( m_pLayout ), pchFontDesc );
		g_free( pchFontDesc );
	}

	int iLineOffset = 0;
	while ( ( pLayoutLine = pango_layout_iter_get_line_readonly( pIter ) ) != NULL )
	{
		// Get the render position in the full layout
		int yPURunBaseline = pango_layout_iter_get_baseline( pIter );

#ifdef LINEDEBUG
		s_idLineDebug++;
#endif

		// For each run in the line...
		PangoLayoutRun *pLayoutRun;
		while ( ( pLayoutRun = pango_layout_iter_get_run_readonly( pIter ) ) != NULL )
		{
			// Get the extents of this run
			PangoRectangle rcPURunLogical, rcPURunInk;
			pango_layout_iter_get_run_extents( pIter, &rcPURunInk, &rcPURunLogical );
			if (rcPURunLogical.width > 0 && rcPURunLogical.height > 0 && rcPURunLogical.width < nMaxStringSizePixels )
			{
				bDrawText = true;

				// Render this run
				TextLayoutPangoBitmapRange_t &bitmapRange = bitmapRanges[ bitmapRanges.AddToTail() ];

				bitmapRange.m_nWidth = 0;
				bitmapRange.m_nRows = 0;
				bitmapRange.m_pBuffer = NULL;
				bitmapRange.m_flTextureHeight = ((float)rcPURunLogical.height) / PANGO_SCALE;
				if( m_flLineSpacing + m_flLineSpacingAscent > 0 ) // only pad the line size if we are making it bigger, don't shrink it, the y-offset below will drag it around as needed
					bitmapRange.m_flTextureHeight += ((m_flLineSpacing + m_flLineSpacingAscent) / PANGO_SCALE);
				bitmapRange.m_flTextureHeight = ceilf( bitmapRange.m_flTextureHeight );
				bitmapRange.m_flStringOffsetX = ceilf( ( float )rcPURunLogical.x / PANGO_SCALE );
				bitmapRange.m_flStringOffsetY = MAX( 0.0f, ceilf( ( float )rcPURunLogical.y / PANGO_SCALE + iYAdjust + ((float)iLine*(m_flLineSpacingAscent+m_flLineSpacing) / PANGO_SCALE) ) );
				bitmapRange.m_iColorIndex = -1;

				if ( bitmapRange.m_flStringOffsetY < m_flMaxHeight )
				{
					// per https://developer.gnome.org/pango/stable/pango-Layout-Objects.html#pango-layout-set-height 
					// if you want a height to apply you MUST have ellipsize set to something other than None
					// so here if we had a maxheight and we want to draw past it, just exclude it

					// Calc the baseline within this small cached piece, not the whole layout, then render.

					// The text decoration line widths are based on the logical run's width (before we've adjusted it)
					const int cyPUTextDecorationLineWidth = rcPURunLogical.width;

					rcPURunLogical.width = MAX( rcPURunLogical.width, rcPURunInk.width - MIN( 0, rcPURunInk.x ) ); // give it enough width to render, for italicized text the ink flows out of the logical bounds

					bitmapRange.m_nWidth = (rcPURunLogical.width + PANGO_SCALE - 1) / PANGO_SCALE;
					bitmapRange.m_nRows = (rcPURunLogical.height + rcPURunLogical.y - iLineOffset + PANGO_SCALE - 1) / PANGO_SCALE;

					DrawRun( pLayoutRun, yPURunBaseline - yPULineTop, bitmapRange, cyPUTextDecorationLineWidth );

					if ( m_flLineSpacingAscent > 0 )
						bitmapRange.m_flStringOffsetY += m_flLineSpacingAscent / PANGO_SCALE;

					// Gets the range of characters that originally spawned the glyphs in the run.
					int curIndex = LayoutByteIndexToCharIndex( pango_layout_iter_get_index( pIter ) );
					bitmapRange.m_iColorIndex = FindNextTextRangeWithColorAndInRange( pFormatProps, cFormatProps, curIndex, curIndex ); // check if this should be a color change
				}
				else
				{
					bitmapRanges.RemoveMultipleFromTail( 1 );
				}
			}

			// More runs on this line?
			if ( !pango_layout_iter_next_run( pIter ) )
				break;
		}

		// Track where the next line is supposed to start, so the baseline within the cached segment can be calculated.
		PangoRectangle rcPULineLogical;
		pango_layout_iter_get_line_extents( pIter, NULL, &rcPULineLogical );
		yPULineTop += rcPULineLogical.height;
		iLineOffset = yPULineTop;
        iYAdjust = 0;
		iLine++;

		// More lines in the layout?
		if ( !pango_layout_iter_next_line( pIter ) )
			break;
	}
	pango_layout_iter_free( pIter );

	if( s_bDebugFontSelection )
	{
		Msg( "\n");
	}

	return bDrawText;
}

unsigned char* CUITextLayoutPango::DrawRun( PangoLayoutRun *pLayoutRun, int yPUBaseline, TextLayoutPangoBitmapRange_t& bitmapRange, int cyPUTextDecorationLineWidth )
{
	VPROF_BUDGET( "CUITextLayoutPango::DrawRun", VPROF_BUDGETGROUP_TENFOOT );

	// the code below for inserting spacing assumes the glyph run width is the bitmap width

	// Alloc a freetype bitmap
	FT_Bitmap bitmap;
	bitmap.width = bitmapRange.m_nWidth;
	bitmap.pitch = bitmapRange.m_nWidth;
	bitmap.rows = bitmapRange.m_nRows;

	int nBitmapBytes = bitmap.pitch * bitmap.rows;
	bitmapRange.m_pBuffer = (uint8 *)malloc( nBitmapBytes );
	bitmap.buffer = bitmapRange.m_pBuffer;

	bitmap.num_grays = 256;
	bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
	memset( bitmap.buffer, 0, nBitmapBytes );

	{
		VPROF_BUDGET( "CUITextLayoutPango::DrawRun - Glyph render", VPROF_BUDGETGROUP_TENFOOT );
		if( s_bDebugFontSelection )
		{
			PangoFontDescription * pDesc = pango_font_describe( pLayoutRun->item->analysis.font );
			char* pchFont = pango_font_description_to_string( pDesc );
			Msg( "\t\"%s\" actually selected for %d glyphs\n", pchFont, pLayoutRun->glyphs->num_glyphs );
			g_free( pchFont );
			pango_font_description_free( pDesc );
		}
		pango_ft2_render_transformed( &bitmap, NULL, pLayoutRun->item->analysis.font, pLayoutRun->glyphs, 0, yPUBaseline );
	}

	// Deal with underline and strikethrough, since pango only does this at the line and layout level apis
	if( HasUnderlineAttr( pLayoutRun ) )
	{
		PangoFontMetrics *pFontMetrics = pango_font_get_metrics( pLayoutRun->item->analysis.font, pLayoutRun->item->analysis.language );
		int cyPUUnderlineThickness = pango_font_metrics_get_underline_thickness( pFontMetrics );
		int yPUUnderlinePosition = pango_font_metrics_get_underline_position( pFontMetrics );
		DrawRect( &bitmap, 0, yPUBaseline, yPUUnderlinePosition, cyPUTextDecorationLineWidth, cyPUUnderlineThickness );
		pango_font_metrics_unref( pFontMetrics );
	}

	if( HasStrikethroughAttr( pLayoutRun ) )
	{
		PangoFontMetrics *pFontMetrics = pango_font_get_metrics( pLayoutRun->item->analysis.font, pLayoutRun->item->analysis.language );
		int cyPUStrikethroughThickness = pango_font_metrics_get_strikethrough_thickness( pFontMetrics );
		int yPUStrikethroughPosition = pango_font_metrics_get_strikethrough_position( pFontMetrics );
		DrawRect( &bitmap, 0, yPUBaseline, yPUStrikethroughPosition, cyPUTextDecorationLineWidth, cyPUStrikethroughThickness );
		pango_font_metrics_unref( pFontMetrics );
	}

#ifdef LINEDEBUG
	char szFilename[128];
	sprintf( szFilename, "out_%d_%d_%d.pgm", s_idLineDebug, bitmap.width, bitmap.rows );
	FILE *pf = fopen( szFilename, "wb" );
	fprintf( pf, "P5\n" "%d %d\n" "255\n", bitmap.width, bitmap.rows );
	for( int nRow = 0; nRow < bitmap.rows; nRow++ )
	{
		fwrite( bitmap.buffer + nRow * bitmap.pitch, 1, bitmap.width, pf );
	}
	fclose( pf );
#endif

	return bitmap.buffer;
}

UITextTextureHandle_t CUITextLayoutPango::UpdateTextureForRun( UITextOpacityMaskDataRange_t& cacheRange, TextLayoutPangoBitmapRange_t& bitmapRange, IUITextTextureStorage *pStorage )
{
	VPROF_BUDGET( "CUITextLayoutPango::UpdateTextureForRun", VPROF_BUDGETGROUP_TENFOOT );

	// the different between the height of the label and the height of the pango bitmap, used when line spacing is used to squash lines together
	int nHeight = (int)bitmapRange.m_flTextureHeight;
	int nHeightDiff = MAX( 0, bitmapRange.m_nRows - nHeight );

	UITextTextureRegion_t alphaTarget = pStorage->GetTextureRegion( bitmapRange.m_nWidth, nHeight );

	cacheRange.m_flTextureWidth = alphaTarget.m_flTextureWidth;
	cacheRange.m_flTextureHeight = alphaTarget.m_flTextureHeight;

	cacheRange.m_x0 = alphaTarget.m_rect.m_iLeft + alphaTarget.m_iPadding;
	cacheRange.m_y0 = alphaTarget.m_rect.m_iTop + alphaTarget.m_iPadding;
	cacheRange.m_x1 = cacheRange.m_x0 + (float)bitmapRange.m_nWidth;
	cacheRange.m_y1 = cacheRange.m_y0 + bitmapRange.m_flTextureHeight;
	cacheRange.m_flStringOffsetX = bitmapRange.m_flStringOffsetX;
	cacheRange.m_flStringOffsetY = bitmapRange.m_flStringOffsetY;
	cacheRange.m_iColorIndex = bitmapRange.m_iColorIndex;
	cacheRange.m_hTexture = alphaTarget.m_hTexture;

	if( alphaTarget.m_hTexture )
	{
		pStorage->StartUpdateFontGlyphTexture( alphaTarget.m_hTexture );

		int yStart = alphaTarget.m_rect.m_iTop;
		int yEnd = alphaTarget.m_rect.m_iBottom;
		int paddedWidth = alphaTarget.m_rect.m_iRight - alphaTarget.m_rect.m_iLeft;
		Assert( s_nZeroFillBufferSize > 0 );
		int yPerPass = s_nZeroFillBufferSize / paddedWidth;

		// First zero the entire region
		if( yPerPass < 1 )
		{
			int xEnd = alphaTarget.m_rect.m_iRight;
			while( yStart < yEnd )
			{
				int xStart = alphaTarget.m_rect.m_iLeft;
				while( xStart < xEnd )
				{
					int xToDo = Min( s_nZeroFillBufferSize, xEnd - xStart );
					pStorage->UpdateFontGlyphTexture( alphaTarget.m_hTexture, xStart, yStart, xToDo, 1, (void*)s_pZeroFillBuffer );
					xStart += xToDo;
				}

				yStart++;
			}
		}
		else
		{
			while( yStart < yEnd )
			{
				int yToDo = Min( yPerPass, yEnd - yStart );
				pStorage->UpdateFontGlyphTexture( alphaTarget.m_hTexture, alphaTarget.m_rect.m_iLeft, yStart, paddedWidth, yToDo, (void*)s_pZeroFillBuffer );
				yStart += yToDo;
			}
		}

		// now draw the glyph itself
		Assert( bitmapRange.m_nWidth <= alphaTarget.m_rect.m_iRight - alphaTarget.m_iPadding - alphaTarget.m_rect.m_iLeft );
		pStorage->UpdateFontGlyphTexture( alphaTarget.m_hTexture, alphaTarget.m_rect.m_iLeft + alphaTarget.m_iPadding, alphaTarget.m_rect.m_iTop + alphaTarget.m_iPadding, bitmapRange.m_nWidth, bitmapRange.m_nRows - nHeightDiff, bitmapRange.m_pBuffer + nHeightDiff * bitmapRange.m_nWidth );

		pStorage->EndUpdateFontGlyphTexture( alphaTarget.m_hTexture );
	}

	// Free the bitmap buffer
	free( bitmapRange.m_pBuffer );
	bitmapRange.m_pBuffer = NULL;

	return alphaTarget.m_hTexture;
}

bool CUITextLayoutPango::HasUnderlineAttr( PangoLayoutRun *pLayoutRun )
{
	bool bUnderline = false;
	for ( GSList *pList = pLayoutRun->item->analysis.extra_attrs; pList != NULL; pList = pList->next )
	{
		PangoAttribute *pAttr = ( PangoAttribute * )pList->data;
		if ( ( int )pAttr->klass->type == PANGO_ATTR_UNDERLINE )
		{
			bUnderline = ( ( ( PangoAttrInt * )pAttr )->value != PANGO_UNDERLINE_NONE );
			break;
		}
	}
	return bUnderline;
}


bool CUITextLayoutPango::HasStrikethroughAttr( PangoLayoutRun *pLayoutRun )
{
	bool bStrikethrough = false;
	for ( GSList *pList = pLayoutRun->item->analysis.extra_attrs; pList != NULL; pList = pList->next )
	{
		PangoAttribute *pAttr = ( PangoAttribute * )pList->data;
		if ( ( int )pAttr->klass->type == PANGO_ATTR_STRIKETHROUGH )
		{
			bStrikethrough = ( ( ( PangoAttrInt * )pAttr )->value != 0 );
			break;
		}
	}
	return bStrikethrough;
}


void CUITextLayoutPango::DrawRect( FT_Bitmap *pBitmap, int xPU, int yPU, int yPUOffset, int cxPU, int cyPU )
{
	// pango draws underlines and strikethroughs when via the line or layout draw apis, but not the
	// run draw apis. Since we're using the run draw apis, we need to draw these ourselves.

	// The horizontal width of the line we draw is clamped to our bitmap and then rounded up
	// to the nearest pixel.
	int cPixelWidth = MIN( pBitmap->width, ( ( cxPU + PANGO_SCALE - 1 ) / PANGO_SCALE ) );

	// Round down for y, round up for y + cy
	int y0 = ( yPU - yPUOffset ) / PANGO_SCALE;
	int y1 = ( yPU - yPUOffset + cyPU + PANGO_SCALE / 2 ) / PANGO_SCALE;

    // Clip
	if ( y0 < 0 )
		y0 = 0;
	if ( y0 >= pBitmap->rows )
		y0 = pBitmap->rows - 1;
	if ( y1 < 0 )
		y1 = 0;
	if ( y1 > pBitmap->rows )
		y1 = pBitmap->rows;

	// Draw. This doesn't do subpixel rendering, but so far it isn't needed - the fonts have pixel aligned offsets and widths
	// for underline and strikethrough.
	for ( int y = y0; y < y1; y++ )
	{
		for ( int x = 0; x < cPixelWidth; x++ )
		{
			*( pBitmap->buffer + y * pBitmap->pitch + x) = 255;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the required size to fully draw the text layout
//-----------------------------------------------------------------------------
void CUITextLayoutPango::GetRequiredSize( float &flWidth, float &flHeight )
{	
	VPROF_BUDGET( "CUITextLayoutPango::GetRequiredSize", VPROF_BUDGETGROUP_TENFOOT );
	AUTO_LOCK( s_PangoMutex );

	// If m_bEllipsisNoWrap is true, ellipsis is on, and word
	// wrap is off. In this case, the height has already been
	// set to 0 - don't overwrite it.
	int nAvailHeight = 0;
	if ( !m_bEllipsisNoWrap )
	{
		nAvailHeight = pango_layout_get_height( m_pLayout );
		pango_layout_set_height( m_pLayout, INT_MAX );
	}

	PangoRectangle rcLogical, rcInk;

	pango_layout_get_pixel_extents( m_pLayout, &rcInk, &rcLogical );
	flWidth = MAX( rcLogical.width, rcInk.width - MIN( 0, rcInk.x ) );

	const int nExpectedLineCount = pango_layout_get_line_count( m_pLayout );	// Remember our expected line count (pre-shrinking/pre-ellipsize/etc.)
	
	if ( m_nLetterSpacing )
		flWidth += m_nLetterSpacing*2; // room for the front and back of the string
	flHeight = rcLogical.height + m_iFirstLineHeightAdjust + ( ( nExpectedLineCount > 1 ) ? 1.0f : 0.0f ); // taking a Dota 2 hack to solve multi-line labels losing their last line in layout
	if ( m_flLineSpacing+m_flLineSpacingAscent < 0 )
	{
		// if we are shrinking the height only count from the second line on for the size change
		flHeight += ceilf( ( ( m_flLineSpacing + m_flLineSpacingAscent )*( nExpectedLineCount -1) ) / PANGO_SCALE );
	}
	else
	{
		flHeight += ceilf(((m_flLineSpacing+m_flLineSpacingAscent)*nExpectedLineCount )/PANGO_SCALE);
	}

	if ( m_iWidth > 0 && pango_layout_is_wrapped( m_pLayout ) )
	{
		// this hack is from http://src.chromium.org/svn/branches/816/src/ui/gfx/canvas_skia_linux.cc :
		// The text wrapped. There seems to be a bug in Pango when this happens
		// such that the width returned from pango_layout_get_pixel_size is too
		// small. Using the width from pango_layout_get_pixel_size in this case
		// results in wrapping across more lines, which requires a bigger height.
		// As a workaround we use the original width, which is not necessarily
		// exactly correct, but isn't wrong by much.
		//
		// It looks like Pango uses the size of whitespace in calculating wrapping
		// but doesn't include the size of the whitespace when the extents are
		// asked for. See the loop in pango-layout.c process_item that determines
		// where to wrap.

		flWidth = m_iWidth;
	}

	// Only run this workaround for cases where ellipsis is enabled.
	if (( pango_layout_get_ellipsize( m_pLayout ) == PANGO_ELLIPSIZE_END ) &&  (pango_layout_get_wrap( m_pLayout ) == s_eWordWrapMode))
	{
		VPROF_BUDGET( "CUITextLayoutPango::EllipsisCheck", VPROF_BUDGETGROUP_TENFOOT );

		//
		// Work around a bug in pango, sometimes the pango_layout_get_pixel_extents call above
		//  returns a dimension that will still cause ellipsizing if used, so here we
		//  detect that and grow the height by 1 until we can fully fit the rendered text.
		//  Also make sure that we can fit all the lines, otherwise sometimes lines are cut off.
		//
		// The bug is actually caused by the pango ellipsis check using an estimated 
		// line height for the next line. The height calculated by pango_layout_get_pixel_extents
		// is correct, but when that height is applied to the layout, pango reassesses the text line by line
		// and when it gets to the penultimate line sometimes the estimate for the next line height is
		// bigger than the actual size and it incorrectly decides it needs to print an ellipsis.
		// Pango determines the estimated line height by querying the font metrics ascent and descent values
		// which are usually a fairly good estimate. Problems appear most likely with certain chinese fonts. 
		// ( Mingliu font size 18 for example ). 

		int nCurHeight = pango_layout_get_height( m_pLayout );
		int nCurWidth = pango_layout_get_width( m_pLayout );

		if ( m_eAlign == k_ETextAlignUnset || m_eAlign == k_ETextAlignLeft )
			pango_layout_set_width( m_pLayout, flWidth * PANGO_SCALE );

		bool bEllipse = false;
		int nLineCount = 0;
		int nIterationCount = 0;
		do
		{
			pango_layout_set_height( m_pLayout, flHeight * PANGO_SCALE );
			bEllipse = ( bool )( pango_layout_is_ellipsized( m_pLayout ) != 0 );
			nLineCount = pango_layout_get_line_count( m_pLayout ); // re-get line count for the current shrinking iteration
			
			if( bEllipse || (nLineCount < nExpectedLineCount) )
			{
				flHeight += 1.0;
				//Msg( "Increasing height for panel with rectangle %d %d %d %d, ink %d %d %d %d\n", rcLogical.x, rcLogical.y, rcLogical.width, rcLogical.height, rcInk.x, rcInk.y, rcInk.width, rcInk.height );
			}

			++nIterationCount;

		} while ( ( bEllipse || ( nLineCount < nExpectedLineCount ) ) && ( flHeight < m_flMaxHeight ) && ( nIterationCount < 100 ) );

		pango_layout_set_width( m_pLayout, nCurWidth );
		pango_layout_set_height( m_pLayout, nCurHeight );
	}

	if ( !m_bEllipsisNoWrap )
		pango_layout_set_height( m_pLayout, nAvailHeight );
}


//-----------------------------------------------------------------------------
// Return the font ABC widths for the first character of the layout text.
// Some layouts may not be able to return this information.
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::GetCharABCWidths( float *pA, float *pB, float *pC )
{
    if ( !m_pPangoFcFont )
    {
        return false;
    }

	AUTO_LOCK( s_PangoMutex );

	const char *pchUTF8 = pango_layout_get_text( m_pLayout );
	if ( !pchUTF8 || pchUTF8[0] == 0 )
    {
		return false;
    }

    uchar32 uniChar;
    bool bUTF8Error;
    V_UTF8ToUChar32( pchUTF8, uniChar, bUTF8Error );
    if ( bUTF8Error )
    {
        return false;
    }

    FT_Face face = pango_fc_font_lock_face( m_pPangoFcFont );
    if ( !face )
    {
        return false;
    }

	FT_Error error = FT_Load_Glyph( face, FT_Get_Char_Index( face, uniChar ), FT_LOAD_DEFAULT ); 
	if ( error )
    {
		return false;
    }

	FT_Glyph_Metrics metrics = face->glyph->metrics;
	*pA = (float)metrics.horiBearingX / FREETYPE_SCALE;
	*pB = (float)metrics.width / FREETYPE_SCALE;
	*pC = (float)( metrics.horiAdvance - metrics.horiBearingX - metrics.width ) / FREETYPE_SCALE;

    pango_fc_font_unlock_face( m_pPangoFcFont );
    return true;
}


//-----------------------------------------------------------------------------
// Return the kerning between the first and second characters of the layout text.
// Return value is the spacing adjustment when positioning the second
// character after the first.
// Some layouts may not be able to return this information.
//-----------------------------------------------------------------------------
bool CUITextLayoutPango::GetCharKerning( float *pAdjust, float *pAdvance )
{
    if ( !m_pPangoFcFont )
    {
        return false;
    }

	AUTO_LOCK( s_PangoMutex );

	const char *pchUTF8 = pango_layout_get_text( m_pLayout );
	if ( !pchUTF8 || pchUTF8[0] == 0 )
    {
		return false;
    }

    uchar32 uniChars[2];
    bool bUTF8Error;
    pchUTF8 += V_UTF8ToUChar32( pchUTF8, uniChars[0], bUTF8Error );
    if ( bUTF8Error )
    {
        return false;
    }
    V_UTF8ToUChar32( pchUTF8, uniChars[1], bUTF8Error );
    if ( bUTF8Error )
    {
        return false;
    }

    FT_Face face = pango_fc_font_lock_face( m_pPangoFcFont );
    if ( !face )
    {
        return false;
    }

    bool bRet = true;

    FT_UInt nGlyph = FT_Get_Char_Index( face, uniChars[1] );
    if ( FT_HAS_KERNING( face ) )
    {
        FT_Vector ftDelta;
		 
		if ( FT_Get_Kerning( face, FT_Get_Char_Index( face, uniChars[0] ), nGlyph,
                             FT_KERNING_DEFAULT, &ftDelta ) )
        {
            *pAdjust = 0;
            bRet = false;
        }
        else
        {
            *pAdjust = (float)ftDelta.x / FREETYPE_SCALE;
        }
    }
    else
    {
        *pAdjust = 0;
    }

    if ( pAdvance )
    {
        if ( FT_Load_Glyph( face, nGlyph, FT_LOAD_DEFAULT ) )
        {
            *pAdvance = 0;
            bRet = false;
        }
        else
        {
            *pAdvance = (float)face->glyph->advance.x / FREETYPE_SCALE;
        }
    }
    
    pango_fc_font_unlock_face( m_pPangoFcFont );
    return bRet;
}


//-----------------------------------------------------------------------------
// Purpose:  Hit tests a point against the text layout
//  
// unHitRunLength returns the character index hit
// bIsTrailingHit indicates whether the hit is on the leading or trailing side of the char
// bInside is true if the hit is within the text region, when false the position closest the point is returned as
// the character hit offset
//-----------------------------------------------------------------------------
void CUITextLayoutPango::HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString )
{
	VPROF_BUDGET( "CUITextLayoutPango::HitTestPoint", VPROF_BUDGETGROUP_TENFOOT );
	AUTO_LOCK( s_PangoMutex );

	int iIndex, iTrailing;
	bool bInside = pango_layout_xy_to_index( m_pLayout, point.x * PANGO_SCALE , ( point.y - m_iFirstLineHeightAdjust ) * PANGO_SCALE,
                                                   &iIndex, &iTrailing ) ? true : false;

	bIsTrailingHit = iTrailing ? true : false;
	bIsInsideString = bInside;
	unFirstHitOffset = LayoutByteIndexToCharIndex( iIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Convert byte index from pango to char index in string
//-----------------------------------------------------------------------------
uint32 CUITextLayoutPango::LayoutByteIndexToCharIndex( uint32 unByteIndex )
{
	const char *pchUTF8 = pango_layout_get_text( m_pLayout );
	if ( !pchUTF8 || pchUTF8[0] == 0 || unByteIndex == 0 )
		return 0;

#ifdef DBGFLAG_ASSERT
	const char *pchUTF8Start = pchUTF8;
#endif
	int64 nBytesLeft = (int64)unByteIndex;
	uint32 unCharIndex = 0;
	while( nBytesLeft > 0 )
	{
		if ( !pchUTF8[0] )
		{
			AssertMsg2( false, "Layout byte index %u out of range (%u text bytes)",
					   unByteIndex, (uint32)( pchUTF8 - pchUTF8Start ) );
			break;
		}
		
		bool bError;
		uchar32 uVal;
		uint32 unBytes = V_UTF8ToUChar32( pchUTF8, uVal, bError );
		if ( bError )
		{
			// If we have bad string encoding, try to produce as much useful data as possible in case we
			// aren't running in the debugger.
			const char chNulSwap = *pchUTF8;
			*const_cast<char *>( pchUTF8 ) = '\0';

			AssertMsg5( false, "Invalid UTF8 string in CUITextLayoutPango: '%s' [%02x, %02x, %02x, %02x]",
					   pchUTF8Start,
					   chNulSwap,
					   nBytesLeft >= 1 ? pchUTF8[1] : 0x00,
					   nBytesLeft >= 2 ? pchUTF8[2] : 0x00,
					   nBytesLeft >= 3 ? pchUTF8[3] : 0x00 );

			*const_cast<char *>( pchUTF8 ) = chNulSwap;
			break;
		}

		pchUTF8 += unBytes;
		nBytesLeft -= unBytes;
		++unCharIndex;
	}

	return unCharIndex;
}




//-----------------------------------------------------------------------------
// Purpose: Convert char index to byte index as needed by pango often
//-----------------------------------------------------------------------------
uint32 CUITextLayoutPango::CharIndexToLayoutByteIndex( uint32 unCharIndex )
{
	const char *pchUTF8 = pango_layout_get_text( m_pLayout );
	if ( !pchUTF8 || pchUTF8[0] == 0 || unCharIndex == 0 )
		return 0;

#ifdef DBGFLAG_ASSERT
	uint32 unCharIndexStart = unCharIndex;
#endif
	uint32 unByteIndex = 0;
	while( unCharIndex > 0 )
	{
		if ( !pchUTF8[unByteIndex] )
		{
			AssertMsg2( false, "Layout char index %u out of range (%u text chars)",
					   unCharIndexStart, unCharIndexStart - unCharIndex );
			break;
		}

		bool bError;
		uchar32 uVal;
		unByteIndex += V_UTF8ToUChar32( pchUTF8 + unByteIndex, uVal, bError );
		if ( bError )
		{
			AssertMsg( false, "Invalid UTF8 string in CUITextLayoutPango" );
			break;
		}
		
		--unCharIndex;
	}

	return unByteIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Determines the layout coordinates for a given character offset, 
// coordinates are relative to top left of text layout
//-----------------------------------------------------------------------------
void CUITextLayoutPango::GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect )
{
	VPROF_BUDGET( "CUITextLayoutPango::GetCharacterCoordinates", VPROF_BUDGETGROUP_TENFOOT );
	AUTO_LOCK( s_PangoMutex );

	PangoRectangle pos;
	int nByteIndex = CharIndexToLayoutByteIndex( unCharIndex );
	pango_layout_index_to_pos( m_pLayout, nByteIndex, &pos );
	int nLine;
	pango_layout_index_to_line_x( m_pLayout, nByteIndex, false, &nLine, NULL );

	charRegionRect.topLeft.x = pos.x/PANGO_SCALE;
	charRegionRect.topLeft.y = pos.y/PANGO_SCALE + (nLine*m_flLineSpacing)/PANGO_SCALE;
	charRegionRect.bottomRight.x = (pos.x + pos.width)/PANGO_SCALE;
	charRegionRect.bottomRight.y = (pos.y + pos.height)/PANGO_SCALE + m_iFirstLineHeightAdjust + (nLine*m_flLineSpacing)/PANGO_SCALE;
}


//-----------------------------------------------------------------------------
// Purpose: Determines a vector of rects enclosing a range of text, normally 
// used for getting selection highlight regions
//-----------------------------------------------------------------------------
void CUITextLayoutPango::GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects ) 
{
	VPROF_BUDGET( "CUITextLayoutPango::GetCharacterRangeCoordinates", VPROF_BUDGETGROUP_TENFOOT );
	AUTO_LOCK( s_PangoMutex );

	vecRangeRegionRects.RemoveAll();
	vecRangeRegionRects.EnsureCapacity( MAX( unCharStartIndex, unCharEndIndex ) - MIN( unCharStartIndex, unCharEndIndex ) );

	int iStartIndex = CharIndexToLayoutByteIndex( MIN( unCharStartIndex, unCharEndIndex ) );
	int iEndIndex = CharIndexToLayoutByteIndex( MAX( unCharStartIndex, unCharEndIndex ) );

	int iLine = 0;
	{
		//VPROF_BUDGET( "CUITextLayoutPango::pango_layout_index_to_line_x", VPROF_BUDGETGROUP_TENFOOT );
		pango_layout_index_to_line_x( m_pLayout, iStartIndex, 1, &iLine, nullptr );
	}

	int iCurLine = iLine;
	int iCurIndex = iStartIndex;
	int nBottomLastRect = -1;

	while ( iStartIndex < iEndIndex )
	{
		PangoLayoutLine *pLayoutLine = nullptr;
		{
			//VPROF_BUDGET( "CUITextLayoutPango::GetCharacterRangeCoordinates - line", VPROF_BUDGETGROUP_TENFOOT );
			pLayoutLine = pango_layout_get_line_readonly( m_pLayout, iCurLine );
			if ( !pLayoutLine )
				break;

			// move to taking up the whole line and just clamp if needed
			iCurIndex = (pLayoutLine->start_index + pLayoutLine->length);
			if ( iCurIndex > iEndIndex )
				iCurIndex = iEndIndex;
		}
		
		
		PangoRectangle layout_rect;
		{
			//VPROF_BUDGET( "CUITextLayoutPango::GetCharacterRangeCoordinates - pos", VPROF_BUDGETGROUP_TENFOOT );

			pango_layout_index_to_pos( m_pLayout, iStartIndex, &layout_rect );
			pango_extents_to_pixels( nullptr, &layout_rect );
			if ( nBottomLastRect == -1 )
			{
				nBottomLastRect = layout_rect.y + iLine*((m_flLineSpacing + m_flLineSpacingAscent) / PANGO_SCALE);
			}

			layout_rect.y = nBottomLastRect;
			if ( m_flLineSpacing + m_flLineSpacingAscent > 0 ) // only pad the line size if we are making it bigger, don't shrink it, the y-offset below will drag it around as needed
			{
				layout_rect.y = nBottomLastRect; 
				layout_rect.height += 2*((m_flLineSpacing + m_flLineSpacingAscent) / PANGO_SCALE);
			}
		}

		{
			//VPROF_BUDGET( "CUITextLayoutPango::GetCharacterRangeCoordinates - ranges", VPROF_BUDGETGROUP_TENFOOT );

			int* ranges = nullptr;
			int n_ranges = 0;
			pango_layout_line_get_x_ranges( pLayoutLine, iStartIndex, iCurIndex, &ranges, &n_ranges );

			for ( int i = 0; i < n_ranges; ++i )
			{
				int x = PANGO_PIXELS( ranges[2 * i] );
				int width = PANGO_PIXELS( ranges[2 * i + 1] ) - x;

				IUITextLayout::HitTestRegionRect_t &hitRect = vecRangeRegionRects[vecRangeRegionRects.AddToTail()];

				hitRect.topLeft.x = x;
				hitRect.topLeft.y = layout_rect.y;
				hitRect.bottomRight.x = x + width;
				hitRect.bottomRight.y = hitRect.topLeft.y + layout_rect.height;
			}

			g_free( ranges );
		}

		iCurLine++;
		iStartIndex = iCurIndex;
		nBottomLastRect = layout_rect.y + layout_rect.height;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font name for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName )
{
	AUTO_LOCK( s_PangoMutex );

	PangoAttribute *pAttr = pango_attr_family_new( pchFontName );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font size for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize )
{
	AUTO_LOCK( s_PangoMutex );

	PangoAttribute *pAttr = pango_attr_size_new( flFontSize * PANGO_SCALE );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1);
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font style for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle style )
{
	AUTO_LOCK( s_PangoMutex );

	Assert( style != k_EFontStyleUnset );
	PangoAttribute *pAttr = pango_attr_style_new( GetPangoFontStyle( style ) );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font weight for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight weight )
{
	AUTO_LOCK( s_PangoMutex );

	Assert( weight != k_EFontWeightUnset );
	PangoAttribute *pAttr = pango_attr_weight_new( GetPangoFontWeight( weight ) );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Underlines the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline )
{
	AUTO_LOCK( s_PangoMutex );

	PangoAttribute *pAttr = pango_attr_underline_new( bUnderline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Sets strikethrough on the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough )
{
	AUTO_LOCK( s_PangoMutex );

	PangoAttribute *pAttr = pango_attr_strikethrough_new( bStrikethrough );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Indicates that an inline shape should replace the character at that given index
//-----------------------------------------------------------------------------
void CUITextLayoutPango::SetInlineObject( uint32 unCharIndex, float flWidth, float flHeight )
{
	AUTO_LOCK( s_PangoMutex );

	PangoRectangle r;
	r.x = 0;
	r.y = -flHeight * PANGO_SCALE; // Align with the bottom of the text
	r.width = flWidth * PANGO_SCALE;
	r.height = flHeight * PANGO_SCALE;

	PangoAttribute *pAttr = pango_attr_shape_new( &r, &r );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharIndex + 1 );
	SetAttribute( pAttr );
}


//-----------------------------------------------------------------------------
// Purpose: Marks a range of text needing a color. This will affect measurement
//-----------------------------------------------------------------------------
static PangoAttribute *MarkColorRangeAttrCopy( const PangoAttribute *pAttr )
{
	PangoAttribute *pAttrNew = new PangoAttribute;
	pAttrNew->klass = pAttr->klass;
	pAttrNew->start_index = pAttr->start_index;
	pAttrNew->end_index = pAttr->end_index;
	return pAttrNew;
}

static void MarkColorRangeAttrDestroy( PangoAttribute *pAttr )
{
	delete pAttr;
}

static gboolean MarkColorRangeAttrCompare( const PangoAttribute *pAttrA, const PangoAttribute *pAttrB )
{
	return 0;
}

void CUITextLayoutPango::MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex, int iColorIndex )
{
	// Don't need if color index not valid in pango
	if ( iColorIndex == -1 )
		return;

	AUTO_LOCK( s_PangoMutex );

	// Make a custom attribute for MarkColorRangeForMeasurement so this range turns into a run
	static PangoAttrClass klass =
	{
		PANGO_ATTR_INVALID,
		MarkColorRangeAttrCopy,
		MarkColorRangeAttrDestroy,
		MarkColorRangeAttrCompare
	};
	if ( klass.type == PANGO_ATTR_INVALID )
		klass.type = pango_attr_type_register( "MarkColorRange" );

	PangoAttribute *pAttr = new PangoAttribute;
	pango_attribute_init( pAttr, &klass );
	pAttr->start_index = CharIndexToLayoutByteIndex( unCharStartIndex );
	pAttr->end_index = CharIndexToLayoutByteIndex( unCharEndIndex + 1 );
	SetAttribute( pAttr );
}


void CUITextLayoutPango::SetAttribute( PangoAttribute *pAttr )
{
	// If there isn't already an attribute list attached to the layout, alloc and attach, deal with ref counting.
	PangoAttrList *pAttrList = pango_layout_get_attributes( m_pLayout );
	if ( pAttrList == NULL )
	{
		pAttrList = pango_attr_list_new();
		pango_attr_list_insert( pAttrList, pAttr );
		pango_layout_set_attributes( m_pLayout, pAttrList );
		pango_attr_list_unref( pAttrList );
	}
	else
	{
		// In this case, the pAttrList doesn't need to be unref'ed after this.
		pango_attr_list_change( pAttrList, pAttr );
	}

	// force pango to through out any cached data, this fixes a bug with
	// line spacing and colors not interoperating
	pango_layout_context_changed( m_pLayout );
}

