# M3: local 2019 sources. File lists come from the original VPC conditions.
if(BUILD_M5)
  include(${CMAKE_CURRENT_LIST_DIR}/m5-source-lists.cmake)
elseif(BUILD_M4)
  include(${CMAKE_CURRENT_LIST_DIR}/m4-source-lists.cmake)
else()
  include(${CMAKE_CURRENT_LIST_DIR}/m3-source-lists.cmake)
endif()
function(m3_library name type)
  add_library(${name} ${type} ${VPC_${name}_FILES})
  target_link_libraries(${name} PUBLIC source_platform PRIVATE tier0 tier1 mathlib vstdlib interfaces)
  target_compile_definitions(${name} PRIVATE ALLOW_TEXT_MODE=1 VERSION_SAFE_STEAM_API_INTERFACES CSTRIKE_REL_BUILD=1 SURVIVAL_MODE_ENABLED=1)
  if(NOT BUILD_M4 OR name STREQUAL "server" OR name STREQUAL "dedicated" OR name MATCHES "_ds$")
    target_compile_definitions(${name} PRIVATE DEDICATED)
  endif()
  target_include_directories(${name} PRIVATE "${SOURCE_ROOT}" "${SOURCE_ROOT}/${name}"
    "${SOURCE_ROOT}/public/tier2" "${SOURCE_ROOT}/public/tier3")
  set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(type STREQUAL "SHARED")
    target_sources(${name} PRIVATE "${SOURCE_ROOT}/public/tier0/memoverride.cpp" "${SOURCE_ROOT}/public/tier0/crtoverride.cpp")
    target_compile_definitions(${name} PRIVATE MEMOVERRIDE_MODULE=${name})
  endif()
endfunction()
m3_library(tier3 STATIC)
target_include_directories(tier3 PRIVATE "${SOURCE_ROOT}/game/shared")
target_link_libraries(tier3 PRIVATE tier2)
m3_library(dmxloader STATIC)
target_compile_definitions(dmxloader PRIVATE DMXLOADER_LIB)
m3_library(bonesetup STATIC)
m3_library(choreoobjects STATIC)
target_include_directories(choreoobjects PRIVATE "${SOURCE_ROOT}/utils/common")
m3_library(bitmap STATIC)
m3_library(vtf STATIC)
target_link_libraries(vtf PRIVATE bitmap tier2)
# The console AppMain uses no Cocoa launcher manager. Build the POSIX application
# framework, retaining its actual module/system lifecycle and filesystem setup.
if(NOT BUILD_M4 OR BUILD_M6)
  list(FILTER VPC_appframework_FILES EXCLUDE REGEX "cocoamgr\\.mm$")
endif()
if(BUILD_M6)
  list(FILTER VPC_appframework_FILES EXCLUDE REGEX "glmrendererinfo_osx\\.mm$")
endif()
m3_library(appframework STATIC)
target_link_libraries(appframework PRIVATE tier2 tier3 dmxloader )
m3_library(datacache SHARED)
target_compile_definitions(datacache PRIVATE MDLCACHE_DLL_EXPORT)
target_include_directories(datacache PRIVATE "${SOURCE_ROOT}/utils/common")
target_link_libraries(datacache PRIVATE tier2 tier3 bonesetup bitmap vtf dmxloader)
m3_library(studiorender SHARED)
target_compile_definitions(studiorender PRIVATE STUDIORENDER_EXPORTS)
target_include_directories(studiorender PRIVATE "${SOURCE_ROOT}/materialsystem")
target_link_libraries(studiorender PRIVATE tier2 tier3 bitmap vtf)
# macOS's historical VPC conglomerates DX9 and stdshaders into materialsystem.
# The headless executable explicitly requests shaderapiempty (sys_linux.cpp),
# so keep the original material loader/context sources and separate shaderlib.
list(FILTER VPC_materialsystem_FILES EXCLUDE REGEX "/(shaderapidx9|shaderlib|stdshaders)/")
m3_library(materialsystem SHARED)
target_compile_definitions(materialsystem PRIVATE DEFINE_MATERIALSYSTEM_INTERFACE MATERIALSYSTEM_EXPORTS PROTECTED_THINGS_ENABLE)
target_link_libraries(materialsystem PRIVATE tier2 tier3 bitmap vtf)
m3_library(shaderapiempty SHARED)
target_link_libraries(shaderapiempty PRIVATE tier2 tier3)
m3_library(vscript SHARED)
target_compile_definitions(vscript PRIVATE VSCRIPT_DLL_EXPORT LUA_MOD_CASE_INSENSITIVE)
target_include_directories(vscript PRIVATE "${SOURCE_ROOT}/vscript/languages/squirrel/include"
 "${SOURCE_ROOT}/vscript/languages/squirrel/sqplus" "${SOURCE_ROOT}/vscript/languages/lua/lua-5.1.4/src")
target_include_directories(shaderapiempty PRIVATE "${SOURCE_ROOT}/materialsystem")
target_compile_definitions(materialsystem PRIVATE SOURCE_MATERIALSYSTEM_SPLIT)
# This define selects the existing dynamically loaded ShaderAPI branch. Cocoa,
# clocks, files and the rest of macOS still use the real OSX platform definitions.
set(VPC_shaderlib_FILES "${SOURCE_ROOT}/materialsystem/shaderlib/BaseShader.cpp"
 "${SOURCE_ROOT}/materialsystem/shaderlib/ShaderDLL.cpp"
 "${SOURCE_ROOT}/materialsystem/shaderlib/shaderlib_cvar.cpp")
m3_library(shaderlib STATIC)
target_include_directories(shaderlib PRIVATE "${SOURCE_ROOT}/materialsystem")
target_compile_definitions(shaderlib PRIVATE FAST_MATERIALVAR_ACCESS)
target_link_libraries(materialsystem PRIVATE shaderlib)
set(M3_VENDOR "${CMAKE_CURRENT_LIST_DIR}/../vendor/m3/kisak")
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(${CMAKE_CURRENT_LIST_DIR}/protobuf25.cmake)
target_include_directories(vscript PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../generated")
foreach(tgt tier3 dmxloader bonesetup choreoobjects bitmap vtf appframework datacache studiorender materialsystem shaderapiempty vscript shaderlib)
 target_compile_options(${tgt} PRIVATE -Wno-deprecated-declarations -Wno-inconsistent-missing-override -Wno-deprecated-register)
endforeach()
# Generate the game wire formats from the local 2019 .proto files, using an
# arm64 protoc. A single archive owns all message registrations.
set(M3_PROTO_FILES common/netmessages.proto common/network_connection.proto common/engine_gcmessages.proto
 gcsdk/steammessages.proto game/shared/base_gcmessages.proto
 game/shared/cstrike15/cstrike15_gcmessages.proto game/shared/cstrike15/cstrike15_usermessages.proto)
set(M3_PROTO_INCLUDES "--proto_path=${PROTOBUF_ROOT}/src"
 "--proto_path=${SOURCE_ROOT}/common" "--proto_path=${SOURCE_ROOT}/gcsdk"
 "--proto_path=${SOURCE_ROOT}/game/shared" "--proto_path=${SOURCE_ROOT}/game/shared/cstrike15")
set(M3_PROTO_SOURCES)
foreach(proto IN LISTS M3_PROTO_FILES)
 get_filename_component(stem "${proto}" NAME_WE)
 get_filename_component(proto_dir "${SOURCE_ROOT}/${proto}" DIRECTORY)

 list(APPEND M3_PROTO_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../generated/${stem}.pb.cc")
endforeach()
add_library(source_messages STATIC ${M3_PROTO_SOURCES})
target_include_directories(source_messages PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../generated" "${PROTOBUF_ROOT}/src")
target_link_libraries(source_messages PUBLIC libprotobuf)
# Native Steam API loader; use its arm64 slice. Online service initialization is
# disabled through the source's existing NO_STEAM branches in this offline build.
add_library(steam_api STATIC "${CMAKE_CURRENT_LIST_DIR}/../compat/steam_unavailable.cpp")
target_link_libraries(steam_api PRIVATE source_platform)
foreach(tgt appframework materialsystem)
 target_link_libraries(${tgt} PRIVATE steam_api)
endforeach()
foreach(tgt particles soundemittersystem scenefilecache responserules)
 if(tgt STREQUAL "soundemittersystem" OR tgt STREQUAL "scenefilecache")
  m3_library(${tgt} SHARED)
 else()
  m3_library(${tgt} STATIC)
 endif()
 target_link_libraries(${tgt} PRIVATE tier2 tier3 dmxloader)
 target_include_directories(${tgt} PRIVATE "${SOURCE_ROOT}/game/shared" "${SOURCE_ROOT}/responserules/runtime")
endforeach()
# These OSX VPC entries are client capture/mixing implementations. The original
# Linux DEDICATED condition excludes them; use the same server boundary on OSX.
if(NOT BUILD_M4)
  list(FILTER VPC_engine_FILES EXCLUDE REGEX "/(snd_mp3_source|snd_wave_mixer_mp3|voice_mixer_controls_openal|voice_record_openal|voice_record_mac_audioqueue)\\.cpp$")
endif()
list(APPEND VPC_engine_FILES "${SOURCE_ROOT}/engine/sys_linuxwind.cpp")
m3_library(engine SHARED)
target_compile_definitions(engine PRIVATE ENGINE_DLL USE_CONVARS VOICE_OVER_IP BUMPMAP __USEA3D _ADD_EAX_ PROTECTED_THINGS_ENABLE NO_BINK PROTOBUF NO_STEAM)
if(NOT BUILD_M4)
 target_compile_definitions(engine PRIVATE SWDS)
endif()
target_include_directories(engine PRIVATE "${SOURCE_ROOT}/engine/audio" "${SOURCE_ROOT}/engine/audio/private"
 "${SOURCE_ROOT}/engine/audio/private/snd_op_sys" "${SOURCE_ROOT}/engine/audio/public"
 "${M3_VENDOR}/thirdparty/quickhull" "${M3_VENDOR}/external/crypto++-5.61")
target_link_libraries(engine PRIVATE appframework bitmap dmxloader tier2 tier3 vtf source_messages steam_api bz2 z curl)
m3_library(server SHARED)
target_compile_definitions(server PRIVATE GAME_DLL VECTOR PROTECTED_THINGS_ENABLE BOTS CSTRIKE_DLL USE_ECONOMY_FEATURES PROTOBUF NO_STEAM_GAMECOORDINATOR SWDS)
target_include_directories(server PRIVATE "${SOURCE_ROOT}/game/server" "${SOURCE_ROOT}/game/shared" "${SOURCE_ROOT}/game/shared/cstrike15"
 "${SOURCE_ROOT}/game/shared/cstrike15/control" "${SOURCE_ROOT}/game/server/cstrike15" "${SOURCE_ROOT}/game/server/cstrike15/bot"
 "${SOURCE_ROOT}/game/server/cstrike15/bot/states" "${SOURCE_ROOT}/game/shared/cstrike15/bot"
 "${SOURCE_ROOT}/game/server/cstrike15/hostage" "${SOURCE_ROOT}/game/server/cstrike15/control"
 "${SOURCE_ROOT}/game/shared/econ" "${SOURCE_ROOT}/gcsdk/steamextra" "${SOURCE_ROOT}/utils/common"
 "${M3_VENDOR}/thirdparty/libpng-1.5.30")
target_link_libraries(server PRIVATE bitmap bonesetup choreoobjects dmxloader particles responserules tier2 tier3 source_messages steam_api)
foreach(tgt engine server)
 target_compile_options(${tgt} PRIVATE -Wno-deprecated-declarations -Wno-inconsistent-missing-override -Wno-deprecated-register)
 add_dependencies(${tgt} source_messages)
endforeach()

foreach(lua_file IN LISTS VPC_vscript_FILES)
 if(lua_file MATCHES "/lua-5.1.4/.*\\.c$")
  set_source_files_properties("${lua_file}" PROPERTIES LANGUAGE CXX)
 endif()
endforeach()

target_compile_options(engine PRIVATE -include "${CMAKE_CURRENT_LIST_DIR}/../compat/crypto_prelude.h")

# Original .nut custom build step embeds source scripts as NUL-terminated text.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(script spawn_helper vscript_server)

 target_sources(server PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../generated/${script}_nut.h")
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_LIST_DIR}/../generated/init_nut.h"
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../scripts/embed_script.py"
  "${SOURCE_ROOT}/vscript/languages/squirrel/vsquirrel/init.nut" "${CMAKE_CURRENT_LIST_DIR}/../generated/init_nut.h" "g_Script_init"
 DEPENDS "${SOURCE_ROOT}/vscript/languages/squirrel/vsquirrel/init.nut" "${CMAKE_CURRENT_LIST_DIR}/../scripts/embed_script.py" VERBATIM)
target_sources(vscript PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../generated/init_nut.h")
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLE_ASM ON CACHE BOOL "" FORCE)
set(DISABLE_CXXFLAGS_OPTIMIZATIONS ON CACHE BOOL "" FORCE)
add_subdirectory("${M3_VENDOR}/external/crypto++-5.61" "cryptopp")
# Sources listed by the dependency's original quickhull.vpc.
add_library(quickhull STATIC
 "${M3_VENDOR}/thirdparty/quickhull/qhMath.cpp" "${M3_VENDOR}/thirdparty/quickhull/qhMemory.cpp"
 "${M3_VENDOR}/thirdparty/quickhull/qhHalfEdge.cpp" "${M3_VENDOR}/thirdparty/quickhull/qhConvex.cpp"
 "${M3_VENDOR}/thirdparty/quickhull/qhMass.cpp")
target_link_libraries(quickhull PRIVATE source_platform)
target_link_libraries(engine PRIVATE cryptopp-static quickhull)
include(${CMAKE_CURRENT_LIST_DIR}/gns.cmake)
target_link_libraries(engine PRIVATE GameNetworkingSockets)
target_compile_definitions(engine PRIVATE STEAMNETWORKINGSOCKETS_OPENSOURCE)
add_executable(gns_probe "${CMAKE_CURRENT_LIST_DIR}/../probes/gns_probe.cpp")
target_link_libraries(gns_probe PRIVATE source_platform GameNetworkingSockets)
target_compile_definitions(gns_probe PRIVATE STEAMNETWORKINGSOCKETS_OPENSOURCE)
add_test(NAME gns_native_transport_probe COMMAND gns_probe)
set_tests_properties(gns_native_transport_probe PROPERTIES TIMEOUT 30)

if(TARGET cryptopp-object)
 target_compile_options(cryptopp-object PRIVATE -Wno-c++11-narrowing)
else()
 target_compile_options(cryptopp-static PRIVATE -Wno-c++11-narrowing)
endif()

foreach(tgt matchmakingbase_ds matchmaking_ds)
 if(tgt STREQUAL "matchmakingbase_ds")
  m3_library(${tgt} STATIC)
 else()
  m3_library(${tgt} SHARED)
 endif()
 target_compile_definitions(${tgt} PRIVATE MATCHMAKING_DS_DLL SWDS NO_STRING_T VECTOR NO_STEAM_GAMECOORDINATOR PROTECTED_THINGS_ENABLE)
 target_include_directories(${tgt} PRIVATE "${SOURCE_ROOT}/matchmaking" "${SOURCE_ROOT}/game/shared"
  "${SOURCE_ROOT}/gcsdk/steamextra")
 target_link_libraries(${tgt} PRIVATE tier2 tier3 source_messages steam_api)
endforeach()
target_link_libraries(matchmaking_ds PRIVATE matchmakingbase_ds)
# CDedicatedAppSystemGroup's POSIX path uses the text console; all VGUI calls in
# sys_ded are _WIN32 guarded. Do not compile the unused Windows dialog classes.
list(FILTER VPC_dedicated_FILES EXCLUDE REGEX "/(vgui|vgui_controls)/")
m3_library(dedicated SHARED)
target_compile_definitions(dedicated PRIVATE LAUNCHERONLY)
target_include_directories(dedicated PRIVATE "${SOURCE_ROOT}/engine" "${SOURCE_ROOT}/filesystem" "${SOURCE_ROOT}/dedicated/console")
target_link_libraries(dedicated PRIVATE appframework dmxloader tier2 tier3 vpklib steam_api bz2 z curl)
set_target_properties(dedicated PROPERTIES PREFIX "")
add_executable(csgo_dedicated "${SOURCE_ROOT}/dedicated_main/main.cpp")
target_link_libraries(csgo_dedicated PRIVATE source_platform)
add_dependencies(csgo_dedicated dedicated)
include(${CMAKE_CURRENT_LIST_DIR}/image-dependencies.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/gcsdk.cmake)
m3_library(vgui_controls STATIC)
target_link_libraries(vgui_controls PRIVATE tier2 tier3 bitmap steam_api)
target_link_libraries(vgui_controls PRIVATE source_messages)
target_link_libraries(server PRIVATE vgui_controls)
m3_library(kv3lib STATIC)
target_link_libraries(kv3lib PRIVATE tier2)
target_link_libraries(server PRIVATE kv3lib)
add_executable(game_data_probe "${CMAKE_CURRENT_LIST_DIR}/../probes/game_data_probe.cpp")
target_include_directories(game_data_probe PRIVATE "${SOURCE_ROOT}/gcsdk/steamextra")
target_link_libraries(game_data_probe PRIVATE source_platform gcsdk_game kv3lib source_messages tier2 tier1 tier0 vstdlib)
add_test(NAME game_data_native_probe COMMAND game_data_probe "${GAME_RESOURCE_ROOT}/cfg/survival/survival_config.kv3")
set_tests_properties(game_data_native_probe PROPERTIES TIMEOUT 30)

foreach(tgt matchmaking_ds matchmakingbase_ds)
 target_include_directories(${tgt} PRIVATE "${SOURCE_ROOT}/common/xlast_csgo")
endforeach()
add_executable(vscript_probe "${CMAKE_CURRENT_LIST_DIR}/../probes/vscript_probe.cpp")
target_link_libraries(vscript_probe PRIVATE source_platform tier1 tier0 vstdlib)
add_dependencies(vscript_probe vscript)
add_test(NAME vscript_native_binding_probe COMMAND vscript_probe "$<TARGET_FILE:vscript>")
set_tests_properties(vscript_native_binding_probe PROPERTIES TIMEOUT 30)

# Retain original nullable Steam telemetry contracts without creating services.
target_compile_options(server PRIVATE -UNO_STEAM)

# Crypto++ needs glibc byteswap.h, not Source tier1/byteswap.h.
target_include_directories(engine BEFORE PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../compat/linux-system")
