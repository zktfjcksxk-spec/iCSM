#ifndef INCLUDED_RESOURCETYPE_H
#define INCLUDED_RESOURCETYPE_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

typedef uint64 ResourceType_t;

#define RSTY_BYTE_POS( byteVal, shft )	ResourceType_t( uint64(uint8(byteVal)) << uint8(shft * 8) )

// RESOURCE_TYPE_8CC( 'v', 'm', 'o', 'd', 'e', 'l', 0, 0 ) --> 'vmodel'
#define RESOURCE_TYPE_8CC( a, b, c, d, e, f, g, h )	ResourceType_t( RSTY_BYTE_POS(a, 0) | RSTY_BYTE_POS(b, 1) | RSTY_BYTE_POS(c, 2) | RSTY_BYTE_POS(d, 3) | RSTY_BYTE_POS(e, 4) | RSTY_BYTE_POS(f, 5) | RSTY_BYTE_POS(g, 6) | RSTY_BYTE_POS(h, 7) )
#define RESOURCE_TYPE_NONE 0


#define RESOURCE_EXT_MAX_LEN 8
#define RESOURCE_EXT_MIN_BUF_SIZE (RESOURCE_EXT_MAX_LEN+1) // for '\0'


DECLARE_POINTER_HANDLE( HResourceManifest );


enum ResourceManifestLoadBehavior_t
{
	RESOURCE_MANIFEST_LOAD_STREAMING_DATA = 0,
	RESOURCE_MANIFEST_INITIALLY_USE_FALLBACKS,
};

enum ResourceManifestLoadPriority_t
{
	RESOURCE_MANIFEST_LOAD_PRIORITY_DEFAULT = -1,
	RESOURCE_MANIFEST_LOAD_PRIORITY_LOW,
	RESOURCE_MANIFEST_LOAD_PRIORITY_MEDIUM,
	RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH,

	RESOURCE_MANIFEST_LOAD_PRIORITY_COUNT,
};

typedef void( *ResourceManifestLoadCompletionCallback_t )( HResourceManifest hManifest, void *pContext );
typedef void( *ResourceSystemForcedSynchronizationCallback_t )( void *pContext );

enum ProceduralResourceType_t
{
	PROCEDURAL_RESOURCE_ANONYMOUS = 0,	// This means the resource can never be found by name. The name passed into FindOrCreateProceduralResource is used for debugging only and the resource is always created
	PROCEDURAL_RESOURCE_NAMED,			// This means the resource can be found by name. If the named resource exists when FindOrCreateProceduralResource, a handle to the exiting resource is returned
};




#endif // INCLUDED_RESOURCETYPE_H