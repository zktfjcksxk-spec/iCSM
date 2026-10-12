#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "tier1/interface.h"
#include "tier1/checksum_crc.h"
#include "tier1/checksum_sha1.h"
#include "icvar.h"
#include "filesystem.h"
#include "bspfile.h"
#include "phyfile.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "cmodel.h"
#include "vphysics/collision_set.h"
#include "vphysics/virtualmesh.h"
#include "probe_checks.h"

static CreateInterfaceFn factories[3];
static void *Factory(const char *version, int *status)
{
    for (auto factory : factories) if (void *result = factory(version, status)) return result;
    return nullptr;
}
static void AssertFailed(const char *file, int line, const char *message)
{
    std::fprintf(stderr, "ASSERT %s:%d %s\n", file, line, message);
    ++failures;
}
struct Sample { std::string path, sha; unsigned size, crc; std::vector<char> bytes; };
struct Query { std::string kind; Vector start, end, extent, normal; float fraction; };
static objectparams_t DefaultParams()
{
    // game/shared/physics_shared.cpp g_PhysDefaultObjectParams.
    objectparams_t p = {nullptr, 1, 1, .1f, .1f, .05f, "M2", nullptr, 0, 1, true};
    return p;
}
class VirtualMeshFixture : public IVirtualMeshEvent
{
    Vector vertices[8] = {
        Vector(-128,-128,-16), Vector(128,-128,-16), Vector(128,128,-16), Vector(-128,128,-16),
        Vector(-128,-128,0), Vector(128,-128,0), Vector(128,128,0), Vector(-128,128,0)
    };
public:
    int builds=0, queries=0;
    void GetVirtualMesh(void *, virtualmeshlist_t *list) override
    {
        ++builds;
        *list = {};
        list->pVerts=vertices; list->vertexCount=8; list->triangleCount=12; list->indexCount=36;
        const unsigned short indices[36] = {
            0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,1,5, 0,5,4,
            1,2,6, 1,6,5, 2,3,7, 2,7,6, 3,0,4, 3,4,7
        };
        std::copy(indices,indices+36,list->indices);
    }
    void GetWorldspaceBounds(void *, Vector *mins, Vector *maxs) override
    {
        *mins=Vector(-128,-128,-16); *maxs=Vector(128,128,0);
    }
    void GetTrianglesInSphere(void *, const Vector &, float, virtualmeshtrianglelist_t *list) override
    {
        ++queries;
        list->triangleCount=12;
        // The native virtual-mesh solver filters these candidates by actual distance.
        for(int i=0;i<12;++i) list->triangleIndices[i]=i;
    }
};
static void VirtualMeshDynamics(IPhysics *physics, IPhysicsCollision *collision, IPhysicsSurfaceProps *props)
{
    Check(collision->SupportsVirtualMesh(),"native collision API supports virtual meshes");
    VirtualMeshFixture fixture;
    virtualmeshparams_t meshParams = {&fixture,nullptr,true};
    CPhysCollide *ground=collision->CreateVirtualMesh(meshParams);
    Check(ground!=nullptr && fixture.builds>0,"actual CreateVirtualMesh builds its recursive bounding hull");
    if(!ground) return;
    CPhysConvex *parts[2] = {
        collision->BBoxToConvex(Vector(-24,-16,-8),Vector(-8,16,8)),
        collision->BBoxToConvex(Vector(8,-16,-8),Vector(24,16,8))
    };
    Check(parts[0] && parts[1],"actual collision API creates two convex body parts");
    if(!parts[0] || !parts[1]) {
        for(auto part:parts) if(part) collision->ConvexFree(part);
        collision->DestroyCollide(ground); return;
    }
    convertconvexparams_t convert;
    convert.buildOuterConvexHull=true;
    CPhysCollide *compound=collision->ConvertConvexToCollideParams(parts,2,convert);
    Check(compound!=nullptr,"actual compound body has a recursive outer hull");
    if(!compound) { collision->DestroyCollide(ground); return; }
    IPhysicsEnvironment *env=physics->CreateEnvironment();
    Check(env!=nullptr,"virtual-mesh regression creates a native IVP environment");
    if(!env) { collision->DestroyCollide(compound); collision->DestroyCollide(ground); return; }
    env->SetGravity(Vector(0,0,-600)); env->SetSimulationTimestep(1.f/120);
    objectparams_t p=DefaultParams();
    int material=props->GetSurfaceIndex("default");
    auto floor=env->CreatePolyObjectStatic(ground,material,vec3_origin,vec3_angle,&p);
    auto body=env->CreatePolyObject(compound,material,Vector(0,0,7),vec3_angle,&p);
    Check(floor && body,"virtual mesh and overlapping compound become actual IVP objects");
    if(floor && body) {
        body->EnableDrag(false); body->Wake();
        std::printf("VIRTUAL_MESH_SIMULATE initial_z=7 builds=%d\n",fixture.builds);
        for(int step=0;step<1200;++step) env->Simulate(1.f/120);
        Vector after,velocity; body->GetPosition(&after,nullptr); body->GetVelocity(&velocity,nullptr);
        std::printf("VIRTUAL_MESH_REST z=%.9g speed=%.9g builds=%d queries=%d asleep=%d\n",
                    after.z,velocity.Length(),fixture.builds,fixture.queries,body->IsAsleep());
        Check(fixture.queries>0 && std::isfinite(after.z) && std::fabs(after.z-8)<1 && velocity.Length()<1,
              "recursive compound collision descends the actual virtual hull and rests on its triangles");
    }
    physics->DestroyEnvironment(env);
    collision->DestroyCollide(compound); collision->DestroyCollide(ground);
}
class SolidDefaults : public IVPhysicsKeyHandler
{
public:
    void SetDefaults(void *data) override { static_cast<solid_t *>(data)->params=DefaultParams(); }
    void ParseKeyValue(void *, const char *, const char *) override { Check(false,"original model solid contains supported keys"); }
};
class BrushInfo : public IConvexInfo
{
public:
    const dbrush_t *brushes;
    int count;
    unsigned int GetContents(int data) override
    {
        if (data<0 || data>=count) { Check(false,"valid original brush game-data index"); return 0; }
        return brushes[data].contents;
    }
};
static void Queries(IPhysicsCollision *collision, vcollide_t &world, const std::vector<Query> &queries, BrushInfo &brushes)
{
    for (const Query &q : queries) {
        Ray_t ray; ray.Init(q.start, q.end, -q.extent, q.extent);
        trace_t nearest = {}; nearest.fraction = 1;
        for (int i = 0; i < 1; ++i) {
            trace_t hit = {};
            collision->TraceBox(ray, CONTENTS_SOLID, &brushes, world.solids[i], vec3_origin, vec3_angle, &hit);
            if (hit.fraction < nearest.fraction) nearest = hit;
        }
        std::printf("MAP_TRACE %s box=%.0f fraction=%.9g expected=%.9g normal=%.6g,%.6g,%.6g\n",
                    q.kind.c_str(), q.extent.x, nearest.fraction, q.fraction,
                    nearest.plane.normal.x, nearest.plane.normal.y, nearest.plane.normal.z);
        // Engine sweep epsilon is 1/32 inch; independent oracle has no skin.
        Check(nearest.DidHit() && !nearest.startsolid && std::fabs(nearest.fraction-q.fraction)*1024 < .08f &&
              DotProduct(nearest.plane.normal, q.normal) > .999f,
              "real Dust II ray/box agrees with independent BSP brush planes");
    }
    Ray_t miss; miss.Init(Vector(0,0,9000), Vector(0,0,9100));
    bool clear = true;
    for (int i=0; i<world.solidCount; ++i) {
        trace_t hit = {}; collision->TraceBox(miss, world.solids[i], vec3_origin, vec3_angle, &hit);
        clear = clear && !hit.DidHit();
    }
    Check(clear, "real map trace outside world is a miss");
}
static void ModelQueries(IPhysicsCollision *collision, vcollide_t &model)
{
    Vector mins, maxs;
    collision->CollideGetAABB(&mins, &maxs, model.solids[0], vec3_origin, vec3_angle);
    std::printf("PHY_AABB %.6g %.6g %.6g / %.6g %.6g %.6g volume=%.6g\n",
                mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z, collision->CollideVolume(model.solids[0]));
    Vector center=(mins+maxs)*.5f;
    // physics_reference.py independently decoded the original compact ledge:
    // 8 vertices, 12 triangles, +/-32 inches on all axes.
    Check((mins-Vector(-32,-32,-32)).Length()<.001f && (maxs-Vector(32,32,32)).Length()<.001f &&
          std::fabs(collision->CollideVolume(model.solids[0])-262144)<1,
          "real PHY bounds/volume agree with independently decoded original vertices");
    for (float extent : {0.f, 4.f}) {
        Vector start=center, end=center; start.z=maxs.z+64; end.z=mins.z-64;
        Ray_t ray; ray.Init(start, end, Vector(-extent,-extent,-extent), Vector(extent,extent,extent));
        trace_t hit={}; collision->TraceBox(ray, model.solids[0], vec3_origin, vec3_angle, &hit);
        std::printf("PHY_TRACE box=%.0f fraction=%.9g endz=%.9g normalz=%.9g\n",extent,hit.fraction,hit.endpos.z,hit.plane.normal.z);
        Check(hit.DidHit() && !hit.startsolid && std::fabs(hit.endpos.z-(maxs.z+extent))<.08f && hit.plane.normal.z>.999f,
              "original crate PHY ray/box reaches the top plane");
    }
    vcollide_t scaled={}; collision->DuplicateAndScale(&scaled,&model,2.f);
    Vector smin,smax; collision->CollideGetAABB(&smin,&smax,scaled.solids[0],vec3_origin,vec3_angle);
    Check((smin-mins*2).Length()<.02f && (smax-maxs*2).Length()<.02f &&
          std::fabs(collision->CollideVolume(scaled.solids[0])/collision->CollideVolume(model.solids[0])-8)<.01f &&
          scaled.pKeyValues != model.pKeyValues && std::strcmp(scaled.pKeyValues,model.pKeyValues)==0,
          "DuplicateAndScale doubles real PHY geometry, octuples volume and owns its copy");
    collision->VCollideUnload(&scaled);
    Ray_t ray; ray.Init(Vector(0,0,96),Vector(0,0,-96)); trace_t aa={};
    Check(collision->TraceBoxAA(ray,model.solids[0],&aa) && aa.plane.normal.z>.999f,
          "axis-aligned trace adapter runs the real collision solver");
    ray.Init(Vector(128,128,96),Vector(128,128,-96));
    Check(!collision->TraceBoxAA(ray,model.solids[0],&aa),"axis-aligned trace reports a miss");
}
static void Dynamics(IPhysics *physics, IPhysicsCollision *collision, IPhysicsSurfaceProps *props,
                     vcollide_t &world, vcollide_t &model, const Query &floor)
{
    int material=props->GetSurfaceIndex("default");
    objectparams_t p=DefaultParams();
    int surfaceTable[128]={}; bool tableFound=false;
    IVPhysicsKeyParser *parser=collision->VPhysicsKeyParserCreate(&world);
    while (!parser->Finished()) {
        if (!V_stricmp(parser->GetCurrentBlockName(),"materialtable")) {
            parser->ParseSurfaceTable(surfaceTable,nullptr); tableFound=true;
        } else parser->SkipBlock();
    }
    collision->VPhysicsKeyParserDestroy(parser);
    Check(tableFound && surfaceTable[1]==material && surfaceTable[4]==props->GetSurfaceIndex("concrete"),
          "original world material table parses into local surface indices");
    props->SetWorldMaterialIndexTable(surfaceTable,128);
    solid_t solid={}; SolidDefaults defaults; bool solidFound=false;
    parser=collision->VPhysicsKeyParserCreate(&model);
    while (!parser->Finished()) {
        if (!V_stricmp(parser->GetCurrentBlockName(),"solid")) {
            parser->ParseSolid(&solid,&defaults); solidFound=true;
        } else parser->SkipBlock();
    }
    collision->VPhysicsKeyParserDestroy(parser);
    Check(solidFound && solid.index==0 && solid.params.mass==400 && !V_stricmp(solid.surfaceprop,"rock") &&
          std::fabs(solid.params.volume-262143.9375f)<.001f,
          "original PHY parser preserves mass, surface property and volume");
    solid.params.enableCollisions=true; solid.params.pName=solid.name;
    IPhysicsEnvironment *env=physics->CreateEnvironment();
    Check(env && physics->GetActiveEnvironmentByIndex(0)==env, "VPhysics031 creates and tracks a real environment");
    if (!env) return;
    env->SetGravity(Vector(0,0,-600)); env->SetSimulationTimestep(1.f/120);
    // Isolate integration from contacts before adding the original world.
    Vector pos(0,0,100);
    IPhysicsObject *ball=env->CreateSphereObject(8,material,pos,vec3_angle,&p,false);
    Check(ball!=nullptr,"native IVP sphere object");
    float zero=0; ball->EnableDrag(false); ball->SetDamping(&zero,&zero); ball->Wake();
    Vector gravity; env->GetGravity(&gravity);
    std::printf("CLOCK start=%.9g step=%.9g gravity=%.9g\n",env->GetSimulationTime(),env->GetSimulationTimestep(),gravity.z);
    for(int step=0;step<60;++step) env->Simulate(1.f/120);
    Vector after,velocity; ball->GetPosition(&after,nullptr); ball->GetVelocity(&velocity,nullptr);
    float elapsed=env->GetSimulationTime(), timestep=env->GetSimulationTimestep();
    float idealZ=pos.z-300*elapsed*elapsed;
    std::printf("FREEFALL time=%.9g z=%.9g expected=%.9g velocity=%.9g\n",elapsed,after.z,idealZ,velocity.z);
    Check(std::fabs(after.z-idealZ)<600*elapsed*timestep+300*timestep*timestep &&
          std::fabs(velocity.z+600*elapsed)<600*timestep,
          "native rigid-body fall matches gravity/time within one simulation step");
    env->DestroyObject(ball);
    // solid[0] is the world. Other original solids are water/playerclip/monsterclip;
    // game/shared/physics_shared.cpp assigns them separate contents and controllers.
    for (int i=0;i<1;++i)
        Check(env->CreatePolyObjectStatic(world.solids[i],material,vec3_origin,vec3_angle,&p)!=nullptr,
              "real map compact solid becomes a static IVP object");
    pos=floor.start; // The original trace has verified this point is in empty space.
    ball=env->CreateSphereObject(8,material,pos,vec3_angle,&p,false);
    ball->EnableDrag(false); ball->Wake();
    for(int step=0;step<1800;++step) env->Simulate(1.f/120);
    ball->GetPosition(&after,nullptr); ball->GetVelocity(&velocity,nullptr);
    float floorZ=floor.start.z+(floor.end.z-floor.start.z)*floor.fraction;
    std::printf("MAP_REST z=%.9g floor=%.9g speed=%.9g asleep=%d\n",after.z,floorZ,velocity.Length(),ball->IsAsleep());
    Check(std::fabs(after.z-(floorZ+8))<1 && velocity.Length()<1 && ball->IsAsleep(),
          "rigid body rests and sleeps on the real Dust II floor");
    env->DestroyObject(ball);
    Vector mins,maxs; collision->CollideGetAABB(&mins,&maxs,model.solids[0],vec3_origin,vec3_angle);
    IPhysicsObject *crate=env->CreatePolyObject(model.solids[0],props->GetSurfaceIndex(solid.surfaceprop),pos,vec3_angle,&solid.params);
    Check(crate!=nullptr,"real original PHY becomes a dynamic IVP object");
    crate->EnableDrag(false); crate->Wake();
    for(int step=0;step<2400;++step) env->Simulate(1.f/120);
    crate->GetPosition(&after,nullptr); crate->GetVelocity(&velocity,nullptr);
    std::printf("PHY_REST z=%.9g expected=%.9g speed=%.9g asleep=%d\n",after.z,floorZ-mins.z,velocity.Length(),crate->IsAsleep());
    Check(std::fabs(after.z-(floorZ-mins.z))<1 && velocity.Length()<1 && crate->IsAsleep(),
          "original model PHY falls, contacts the map and settles");
    physics->DestroyEnvironment(env);
    Check(physics->GetActiveEnvironmentByIndex(0)==nullptr,"environment destroys its objects and unregisters");

    // Test elastic response separately with explicit material parameters; original assets remain untouched.
    props->ParseSurfaceData("m2_elastic_fixture", "m2_elastic { base default elasticity 0.9 friction 0.5 }");
    material=props->GetSurfaceIndex("m2_elastic"); env=physics->CreateEnvironment();
    env->SetGravity(Vector(0,0,-600)); env->SetSimulationTimestep(1.f/120);
    CPhysCollide *ground=collision->BBoxToCollide(Vector(-128,-128,-16),Vector(128,128,0));
    env->CreatePolyObjectStatic(ground,material,vec3_origin,vec3_angle,&p);
    ball=env->CreateSphereObject(8,material,Vector(0,0,100),vec3_angle,&p,false);
    ball->EnableDrag(false); ball->Wake();
    bool fell=false,bounced=false;
    for(int step=0;step<240;++step) {
        env->Simulate(1.f/120); ball->GetVelocity(&velocity,nullptr);
        fell=fell || velocity.z < -100; bounced=bounced || (fell && velocity.z > 100);
    }
    Check(bounced,"IVP material elasticity produces a measurable upward rebound");
    physics->DestroyEnvironment(env); collision->DestroyCollide(ground);
}
int main(int argc,char **argv)
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    bool virtualOnly=argc==5 && std::string(argv[4])=="--virtual-only";
    if(argc!=8 && !virtualOnly) { std::fprintf(stderr,"usage: physics_probe vphysics filesystem vstdlib [--virtual-only | csgo-dir samples.tsv queries.tsv cycles]\n"); return 2; }
    DeclareCurrentThreadIsMainThread(); CommandLine()->CreateCmdLine("physics_probe -allowdebug");
    SetAssertDialogDisabled(true); SetAssertFailedNotifyFunc(AssertFailed);
    MathLib_Init(2.2f,2.2f,0.f,2.f,false,false,false);
    CSysModule *modules[3]={Sys_LoadModule(argv[3]),Sys_LoadModule(argv[2]),Sys_LoadModule(argv[1])};
    for(int i=0;i<3;++i) { Check(modules[i]!=nullptr,"native engine module loads"); if(!modules[i])return 1; factories[i]=Sys_GetFactory(modules[i]); }
    ICvar *cv=static_cast<ICvar *>(Factory(CVAR_INTERFACE_VERSION,nullptr));
    IFileSystem *fs=static_cast<IFileSystem *>(Factory(FILESYSTEM_INTERFACE_VERSION,nullptr));
    IPhysics *physics=static_cast<IPhysics *>(Factory(VPHYSICS_INTERFACE_VERSION,nullptr));
    IPhysicsCollision *collision=static_cast<IPhysicsCollision *>(Factory(VPHYSICS_COLLISION_INTERFACE_VERSION,nullptr));
    IPhysicsSurfaceProps *props=static_cast<IPhysicsSurfaceProps *>(Factory(VPHYSICS_SURFACEPROPS_INTERFACE_VERSION,nullptr));
    Check(cv && fs && physics && collision && props,"local 2019 interfaces VPhysics031 / Collision007 / SurfaceProps001");
    if(!cv||!fs||!physics||!collision||!props)return 1;
    Check(cv->Connect(Factory)&&cv->Init()==INIT_OK,"actual cvar starts");
    Check(fs->Connect(Factory)&&fs->Init()==INIT_OK,"actual filesystem starts");
    Check(physics->Connect(Factory)&&physics->Init()==INIT_OK,"actual physics app system starts");
    if(virtualOnly) {
        props->ParseSurfaceData("virtual_mesh_regression","default { density 1000 friction 0.5 elasticity 0.0 }");
        VirtualMeshDynamics(physics,collision,props);
        CheckNativeImages(); physics->Shutdown(); physics->Disconnect(); fs->Shutdown(); fs->Disconnect(); cv->Shutdown(); cv->Disconnect();
        for(int i=2;i>=0;--i)Sys_UnloadModule(modules[i]);
        std::printf("PHYSICS_VIRTUAL_RESULT failures=%d\n",failures); return failures?1:0;
    }
    std::string root=argv[4]; fs->AddVPKFile((root+"/pak01.vpk").c_str()); fs->AddSearchPath(root.c_str(),"GAME");
    std::ifstream sampleInput(argv[5]); std::vector<Sample> samples; Sample s; std::string crc;
    while(sampleInput>>s.path>>s.size>>s.sha>>crc) {
        s.crc=std::stoul(crc,nullptr,16); FileHandle_t file=fs->Open(s.path.c_str(),"rb");
        Check(file!=FILESYSTEM_INVALID_HANDLE,s.path.c_str()); if(file==FILESYSTEM_INVALID_HANDLE)return 1;
        s.bytes.resize(s.size+1); int amount=fs->Read(s.bytes.data(),s.size,file); fs->Close(file);
        CSHA1 hash; hash.Update(reinterpret_cast<unsigned char *>(s.bytes.data()),s.size); hash.Final(); char text[41]; hash.GetHashHex(text,sizeof(text));
        CRC32_t actual; CRC32_Init(&actual); CRC32_ProcessBuffer(&actual,s.bytes.data(),s.size); CRC32_Final(&actual);
        Check(amount==static_cast<int>(s.size)&&V_stricmp(s.sha.c_str(),text)==0&&actual==s.crc,"original sample bytes match independent SHA1/CRC32"); samples.push_back(s);
    }
    Check(samples.size()==4,"four original M2 samples"); if(samples.size()!=4)return 1;
    Check(props->ParseSurfaceData(samples[3].path.c_str(),samples[3].bytes.data())>100,"original CS surface-property file parses");
    VirtualMeshDynamics(physics,collision,props);
    Check(props->GetMaterialIndexDataOps()!=nullptr,"local material save/restore interface returns real serializer");
    std::printf("SURFACES count=%d size=%zu audio=%zu game=%zu\n",props->SurfacePropCount(),sizeof(surfacedata_t),sizeof(surfaceaudioparams_t),sizeof(surfacegameprops_t));
    int defaultIndex=props->GetSurfaceIndex("default");
    Check(defaultIndex>=0 && props->GetSurfaceData(defaultIndex)->game.penetrationModifier==1.f,
          "surface penetration modifier is numeric, not a string-table handle");
    const surfacedata_t *defaultSurface=props->GetSurfaceData(defaultIndex);
    Check(defaultSurface->audio.lowPitchOcclusion==0 && defaultSurface->audio.midPitchOcclusion==15 &&
          defaultSurface->audio.highPitchOcclusion==1 && defaultSurface->game.damageModifier==.5f,
          "original audio occlusion and damage fields preserve local 2019 values");
    int hidden=props->GetSurfaceIndex("slowgrass");
    std::printf("SLOWGRASS index=%d hidetargetid=%d\n",hidden,hidden>=0?props->GetSurfaceData(hidden)->game.hidetargetid:0);
    Check(hidden>=0 && props->GetSurfaceData(hidden)->game.hidetargetid,"local hidetargetid field loads from original asset");
    std::ifstream queryInput(argv[6]); std::vector<Query> queries; Query q;
    while(queryInput>>q.kind>>q.start.x>>q.start.y>>q.start.z>>q.end.x>>q.end.y>>q.end.z>>q.extent.x>>q.extent.y>>q.extent.z>>q.fraction>>q.normal.x>>q.normal.y>>q.normal.z)queries.push_back(q);
    Check(queries.size()==4,"independent floor/wall ray and box reference cases"); if(queries.size()!=4)return 1;
    const BSPHeader_t *bsp=reinterpret_cast<const BSPHeader_t *>(samples[0].bytes.data());
    const lump_t &lump=bsp->lumps[LUMP_PHYSCOLLIDE];
    const char *base=samples[0].bytes.data()+lump.fileofs;
    dphysmodel_t map; std::memcpy(&map,base,sizeof(map));
    phyheader_t header; std::memcpy(&header,samples[1].bytes.data(),sizeof(header));
    Check(map.modelIndex==0&&map.solidCount>0&&header.size==16&&header.solidCount>0,"actual BSP world and PHY header layout");
    int cycles=std::stoi(argv[7]);
    for(int cycle=0;cycle<cycles;++cycle) {
        vcollide_t world={},model={};
        collision->VCollideLoad(&world,map.solidCount,base+sizeof(map),map.dataSize+map.keydataSize);
        collision->VCollideLoad(&model,header.solidCount,samples[1].bytes.data()+header.size,samples[1].size-header.size);
        bool solids=true; for(int i=0;i<world.solidCount;++i)solids=solids&&world.solids[i]; for(int i=0;i<model.solidCount;++i)solids=solids&&model.solids[i];
        Check(solids,"all original world and model compact solids deserialize"); if(!solids)return 1;
        collision->VCollideCheck(&world,"de_dust2 world"); collision->VCollideCheck(&model,"du_crate_64x64_stone");
        Check(true,"real compact-surface topology/ranges pass VCollideCheck");
        if(cycle==0) {
            BrushInfo brushes; const lump_t &brushLump=bsp->lumps[LUMP_BRUSHES];
            brushes.brushes=reinterpret_cast<const dbrush_t *>(samples[0].bytes.data()+brushLump.fileofs); brushes.count=brushLump.filelen/sizeof(dbrush_t);
            Queries(collision,world,queries,brushes); ModelQueries(collision,model);
            vcollide_t scaledWorld={}; collision->DuplicateAndScale(&scaledWorld,&world,2);
            collision->VCollideCheck(&scaledWorld,"scaled Dust II world");
            std::vector<Query> scaledQueries=queries;
            for (Query &query : scaledQueries) { query.start*=2; query.end*=2; query.extent*=2; }
            Queries(collision,scaledWorld,scaledQueries,brushes);
            collision->VCollideUnload(&scaledWorld);
            Dynamics(physics,collision,props,world,model,queries[0]);
        }
        Check(collision->VCollideAllocUserData(&model,64)!=nullptr,"vcollide cache data uses native allocator");
        collision->VCollideFreeUserData(&model);
        collision->VCollideAllocUserData(&model,64); // Unload owns this allocation too.
        collision->VCollideUnload(&model); collision->VCollideUnload(&world);
        Check(!model.solids&&!world.solids&&!model.pKeyValues&&!world.pKeyValues,"world and model collision load/unload ownership");
    }
    auto set=physics->FindOrCreateCollisionSet(0xfedcba98u,32); set->EnableCollisions(2,31);
    Check(physics->FindCollisionSet(0xfedcba98u)==set&&set->ShouldCollide(2,31)&&!set->ShouldCollide(2,3),"local 32-bit collision-set ID interface"); physics->DestroyAllCollisionSets();
    CheckNativeImages(); physics->Shutdown(); physics->Disconnect(); fs->Shutdown(); fs->Disconnect(); cv->Shutdown(); cv->Disconnect();
    for(int i=2;i>=0;--i)Sys_UnloadModule(modules[i]);
    std::printf("PHYSICS_RESULT cycles=%d failures=%d\n",cycles,failures); return failures?1:0;
}
