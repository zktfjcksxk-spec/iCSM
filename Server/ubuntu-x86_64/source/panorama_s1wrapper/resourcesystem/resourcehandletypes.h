//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Forward declares resource types defined by various system libraries
// $NoKeywords: $
//===========================================================================//

#ifndef RESOURCE_HANDLE_TYPES_H
#define RESOURCE_HANDLE_TYPES_H
#pragma once

#include "resourcefile/resourcetype.h"
#include "resourcesystem/stronghandle.h"
#include "resourcesystem/weakhandle.h"

#define DECLARE_RESOURCE_TYPE(x,y,z) 

class IParticleSystemDefinition;
#define RESOURCE_TYPE_PARTICLE_SYSTEM RESOURCE_TYPE_8CC( 'v', 'p', 'c', 'f', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( IParticleSystemDefinition, RESOURCE_TYPE_PARTICLE_SYSTEM, HParticleSystemDefinition );
#define PARTICLE_SYSTEM_HANDLE_INVALID ( HParticleSystemDefinition::StaticInvalidHandle() )

class CAnimationResource;
#define RESOURCE_TYPE_ANIMATION RESOURCE_TYPE_8CC( 'v', 'a', 'n', 'i', 'm', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CAnimationResource, RESOURCE_TYPE_ANIMATION, HAnimation );
#define ANIMATION_HANDLE_INVALID ( HAnimation::StaticInvalidHandle() )

class CAnimationGroupResource;
#define RESOURCE_TYPE_ANIMATION_GROUP RESOURCE_TYPE_8CC( 'v', 'a', 'g', 'r', 'p', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CAnimationGroupResource, RESOURCE_TYPE_ANIMATION_GROUP, HAnimationGroup );
#define ANIMATION_GROUP_HANDLE_INVALID ( HAnimationGroup::StaticInvalidHandle() )

class CSequenceGroupResource;
#define RESOURCE_TYPE_SEQUENCE_GROUP RESOURCE_TYPE_8CC( 'v', 's', 'e', 'q', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CSequenceGroupResource, RESOURCE_TYPE_SEQUENCE_GROUP, HSequenceGroup );
#define SEQUENCE_GROUP_HANDLE_INVALID ( HSequenceGroup::StaticInvalidHandle() )

struct VBitmapFontRuntimeData_t;
#define RESOURCE_TYPE_BITMAP_FONT RESOURCE_TYPE_8CC( 'v', 'f', 'o', 'n', 't', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( VBitmapFontRuntimeData_t, RESOURCE_TYPE_BITMAP_FONT, HBitmapFont );
#define BITMAP_FONT_HANDLE_INVALID ( HBitmapFont::StaticInvalidHandle() )

class IMaterial2;
#define RESOURCE_TYPE_MATERIAL RESOURCE_TYPE_8CC( 'v', 'm', 'a', 't', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( IMaterial2, RESOURCE_TYPE_MATERIAL, HMaterial );
#define MATERIAL_HANDLE_INVALID ( HMaterial::StaticInvalidHandle() )

class CMorphSet;
#define RESOURCE_TYPE_MORPHSET_PREDEFINED RESOURCE_TYPE_8CC( 'v', 'm', 'o', 'r', 'f', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CMorphSet, RESOURCE_TYPE_MORPHSET_PREDEFINED, HMorphSet ); 
#define MORPH_SET_HANDLE_INVALID ( HMorphSet::StaticInvalidHandle() )

class CRenderMesh;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_RENDERMESH RESOURCE_TYPE_8CC( 'v', 'm', 'e', 's', 'h', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CRenderMesh, RESOURCE_TYPE_RENDERMESH, HRenderMesh );
#define RENDERABLE_HANDLE_INVALID ( HRenderMesh::StaticInvalidHandle() )

class CModel;
#define RESOURCE_TYPE_MODEL RESOURCE_TYPE_8CC( 'v', 'm', 'd', 'l', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CModel, RESOURCE_TYPE_MODEL, HModel );
#define MODEL_HANDLE_INVALID ( HModel::StaticInvalidHandle() )

// Define a fake resource type for .vgraf files.
struct ProcessingGraph_t;
#define RESOURCE_TYPE_PROCESSING_GRAPH RESOURCE_TYPE_8CC( 'v', 'g', 'r', 'a', 'f', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( ProcessingGraph_t, RESOURCE_TYPE_PROCESSING_GRAPH, HProcessingGraph );
#define PROCESSING_GRAPH_HANDLE_INVALID ( HProcessingGraph::StaticInvalidHandle() )

// Define a fake resource type for graph instances.
struct ProcessingGraphInstance_t;
#define RESOURCE_TYPE_PROCESSING_GRAPH_INSTANCE RESOURCE_TYPE_8CC( 'v', 'p', 'r', 'a', 'm', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( ProcessingGraphInstance_t, RESOURCE_TYPE_PROCESSING_GRAPH_INSTANCE, HProcessingGraphInstance );
#define PROCESSING_GRAPH_INSTANCE_HANDLE_INVALID ( HProcessingGraphInstance::StaticInvalidHandle() )

class CTextureBase;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_TEXTURE RESOURCE_TYPE_8CC( 'v', 't', 'e', 'x', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CTextureBase, RESOURCE_TYPE_TEXTURE, HRenderTexture );
#define RENDER_TEXTURE_HANDLE_INVALID ( HRenderTexture::StaticInvalidHandle() )

class CVectorGraphic;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_VECTOR_GRAPHIC RESOURCE_TYPE_8CC( 'v', 's', 'v', 'g', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CVectorGraphic, RESOURCE_TYPE_VECTOR_GRAPHIC, HVectorGraphic );
#define VECTOR_GRAPHIC_HANDLE_INVALID ( HVectorGraphic::StaticInvalidHandle() )

struct VGameSoundScript_t;
#define RESOURCE_TYPE_GAMESOUNDSCRIPT RESOURCE_TYPE_8CC( 'v', 'g', 's', 'c', 'r', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( VGameSoundScript_t, RESOURCE_TYPE_GAMESOUNDSCRIPT, HGameSoundScript );
#define GAMESOUNDSCRIPT_HANDLE_INVALID ( HGameSoundScript::StaticInvalidHandle() )

struct VSoundEventScript_t;
class CNameListEvents;
#define RESOURCE_TYPE_SOUNDEVENTSCRIPT RESOURCE_TYPE_8CC( 'v', 's', 'n', 'd', 'e', 'v', 't', 's' )
DECLARE_RESOURCE_TYPE( CNameListEvents, RESOURCE_TYPE_SOUNDEVENTSCRIPT, HSoundEventScript );
#define SOUNDEVENTSCRIPT_HANDLE_INVALID ( HSoundEventScript::StaticInvalidHandle() )

struct VSoundStackScript_t;
class CNameListStacks;
#define RESOURCE_TYPE_SOUNDSTACKSCRIPT RESOURCE_TYPE_8CC( 'v', 's', 'n', 'd', 's', 't', 'c', 'k' )
DECLARE_RESOURCE_TYPE( CNameListStacks, RESOURCE_TYPE_SOUNDSTACKSCRIPT, HSoundStackScript );
#define SOUNDSTACKSCRIPT_HANDLE_INVALID ( HSoundStackScript::StaticInvalidHandle() )

struct VSound_t;
#define RESOURCE_TYPE_SOUND RESOURCE_TYPE_8CC( 'v', 's', 'n', 'd', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( VSound_t, RESOURCE_TYPE_SOUND, HSound );
#define SOUND_HANDLE_INVALID ( HSound::StaticInvalidHandle() )

// phzc is now phys. So if we need level to be its own resource again, we'll need to rename it
class CVPhysXLevelData;
#define RESOURCE_TYPE_VPHYSX_LEVEL RESOURCE_TYPE_8CC( 'v', 'p', 'l', 'e', 'v', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CVPhysXLevelData, RESOURCE_TYPE_VPHYSX_LEVEL, HVPhysXLevel );

class CPhysAggregateData;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_VPHYSX_PHYSICSDATA RESOURCE_TYPE_8CC( 'v', 'p', 'h', 'y', 's', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPhysAggregateData, RESOURCE_TYPE_VPHYSX_PHYSICSDATA, HPhysAggregateData );
#define VPHYSX_PHYSICSDATA_HANDLE_INVALID ( HPhysAggregateData::StaticInvalidHandle() )

class CVPhysXSurfacePropertiesList;
#define RESOURCE_TYPE_VPHYSX_SURFACE_PROPERTIES RESOURCE_TYPE_8CC( 'v', 's', 'u', 'r', 'f', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CVPhysXSurfacePropertiesList, RESOURCE_TYPE_VPHYSX_SURFACE_PROPERTIES, HVPhysXSurfacePropertiesList );
#define VPHYSX_SURFACE_PROPERTIES_HANDLE_INVALID ( HVPhysXSurfacePropertiesList::StaticInvalidHandle() )

class CEntityLump;
#define RESOURCE_TYPE_ENTITY_LUMP RESOURCE_TYPE_8CC( 'v', 'e', 'n', 't', 's', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CEntityLump, RESOURCE_TYPE_ENTITY_LUMP, HEntityLump );
#define ENTITY_LUMP_HANDLE_INVALID ( HEntityLump::StaticInvalidHandle() )
				  
class CLightTree;
#define RESOURCE_TYPE_LIGHT_TREE RESOURCE_TYPE_8CC( 'v', 'l', 't', 'r', 'e', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CLightTree, RESOURCE_TYPE_LIGHT_TREE, HLightTree );
#define LIGHT_TREE_HANDLE_INVALID ( HLightTree::StaticInvalidHandle() )

struct PRTMatrixData_t;
#define RESOURCE_TYPE_PRT_MATRIX RESOURCE_TYPE_8CC( 'v', 'p', 'r', 't', 'm', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( PRTMatrixData_t, RESOURCE_TYPE_PRT_MATRIX, HPRTMatrix );
#define PRT_MATRIX_HANDLE_INVALID ( HPRTMatrix::StaticInvalidHandle() )

class CVirtualVolumeTexture;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_VIRTUAL_VOLUME_TEXTURE RESOURCE_TYPE_8CC( 'v', 'v', 'v', 't', 'x', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CVirtualVolumeTexture, RESOURCE_TYPE_VIRTUAL_VOLUME_TEXTURE, HVirtualVolumeTexture );
#define VIRTUAL_VOLUME_TEXTURE_HANDLE_INVALID ( HVirtualVolumeTexture::StaticInvalidHandle() )

class CWorldNode;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_WORLD_NODE RESOURCE_TYPE_8CC( 'v', 'w', 'n', 'o', 'd', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CWorldNode, RESOURCE_TYPE_WORLD_NODE, HWorldNode );
#define WORLD_NODE_HANDLE_INVALID ( HWorldNode::StaticInvalidHandle() )

struct World_t;	// Forward declaration of associated runtime class
#define RESOURCE_TYPE_WORLD RESOURCE_TYPE_8CC( 'v', 'w', 'r', 'l', 'd', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( World_t, RESOURCE_TYPE_WORLD, HWorld );
#define WORLD_HANDLE_INVALID ( HWorld::StaticInvalidHandle() )

class CWorldVisibility;
#define RESOURCE_TYPE_WORLD_VIS RESOURCE_TYPE_8CC( 'v', 'v', 'i', 's', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CWorldVisibility, RESOURCE_TYPE_WORLD_VIS, HWorldVis );
#define WORLD_VIS_HANDLE_INVALID ( HWorldVis::StaticInvalidHandle() )

struct WorldLighting_t;
#define RESOURCE_TYPE_WORLD_LIGHTING RESOURCE_TYPE_8CC( 'v', 'w', 'r', 'l', 't', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( WorldLighting_t, RESOURCE_TYPE_WORLD_LIGHTING, HWorldLighting );
#define WORLD_LIGHTING_HANDLE_INVALID ( HWorldLighting::StaticInvalidHandle() )

struct WorldEnvironmentMaps_t;
#define RESOURCE_TYPE_WORLD_ENVIRONMENT_MAPS RESOURCE_TYPE_8CC( 'v', 'w', 'e', 'n', 'v', 'm', 'a', 'p' )
DECLARE_RESOURCE_TYPE( WorldEnvironmentMaps_t, RESOURCE_TYPE_WORLD_ENVIRONMENT_MAPS, HWorldEnvironmentMaps );
#define WORLD_ENVIRONMENT_MAPS_HANDLE_INVALID ( HWorldEnvironmentMaps::StaticInvalidHandle() )

class CPostProcessingResource;
#define RESOURCE_TYPE_POST_PROCESSING RESOURCE_TYPE_8CC( 'v', 'p', 'o', 's', 't', 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPostProcessingResource, RESOURCE_TYPE_POST_PROCESSING, HPostProcessing );
#define POST_PROCESSING_HANDLE_INVALID ( HPostProcessing::StaticInvalidHandle() )

class CPanoramaStyle;
#define RESOURCE_TYPE_PANORAMA_STYLE RESOURCE_TYPE_8CC( 'v', 'c', 's', 's', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPanoramaStyle, RESOURCE_TYPE_PANORAMA_STYLE, HPanoramaStyle );
#define PANORAMA_STYLE_HANDLE_INVALID ( HPanoramaStyle::StaticInvalidHandle() )

class CPanoramaLayout;
#define RESOURCE_TYPE_PANORAMA_LAYOUT RESOURCE_TYPE_8CC( 'v', 'x', 'm', 'l', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPanoramaLayout, RESOURCE_TYPE_PANORAMA_LAYOUT, HPanoramaLayout );
#define PANORAMA_LAYOUT_HANDLE_INVALID ( HPanoramaLayout::StaticInvalidHandle() )

class CPanoramaDynamicImages;
#define RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES RESOURCE_TYPE_8CC( 'v', 'p', 'd', 'i', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPanoramaDynamicImages, RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES, HPanoramaDynamicImages );
#define PANORAMA_DYNAMIC_IMAGES_HANDLE_INVALID ( HPanoramaDynamicImages::StaticInvalidHandle() )

class CDotaItemDefinitionResource;
#define RESOURCE_TYPE_DOTA_ITEM_DEFINITION RESOURCE_TYPE_8CC( 'i', 't', 'e', 'm', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CDotaItemDefinitionResource, RESOURCE_TYPE_DOTA_ITEM_DEFINITION, HDotaItemDefinitionResoruce );
#define DOTA_ITEM_DEFINITION_HANDLE_INVALID ( HDotaItemDefinitionResoruce::StaticInvalidHandle() )

class CPanoramaScript;
#define RESOURCE_TYPE_PANORAMA_SCRIPT RESOURCE_TYPE_8CC( 'v', 'j', 's', 0, 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CPanoramaScript, RESOURCE_TYPE_PANORAMA_SCRIPT, HPanoramaScript );
#define PANORAMA_SCRIPT_HANDLE_INVALID ( HPanoramaScript::StaticInvalidHandle() )

class CParticleSnapshot;
#define RESOURCE_TYPE_PARTICLE_SNAPSHOT RESOURCE_TYPE_8CC( 'v', 'p', 's', 'f', 0, 0, 0, 0 )
DECLARE_RESOURCE_TYPE( CParticleSnapshot, RESOURCE_TYPE_PARTICLE_SNAPSHOT, HParticleSnapshot );
#define PARTICLE_SNAPSHOT_HANDLE_INVALID ( HParticleSnapshot::StaticInvalidHandle() )


#endif // RESOURCE_HANDLE_TYPES_H
