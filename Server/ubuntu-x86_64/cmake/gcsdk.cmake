# The 2019 tree supplies public GC contracts but omits their implementations.
# Compile the pinned implementation against those local contracts. The offline
# source branch never creates a GC client, so no coordinator transport is built.
set(M3_GCSDK_FILES gclogger gcmsg messagelist msgprotobuf netpacket
 netpacketpool protobufsharedobject sharedobject sharedobjectcache)
list(TRANSFORM M3_GCSDK_FILES PREPEND "${M3_VENDOR}/gcsdk/")
list(TRANSFORM M3_GCSDK_FILES APPEND ".cpp")
add_library(gcsdk_game STATIC ${M3_GCSDK_FILES}
 "${M3_VENDOR}/gcsdk/steamextra/misc.cpp"
 "${SOURCE_ROOT}/common/steamid.cpp"
 "${M3_VENDOR}/gcsdk/steamextra/tier1/hashglobals.cpp"
 "${SOURCE_ROOT}/gcsdk/steamextra/tier1/tsmempool.cpp"
 "${SOURCE_ROOT}/gcsdk/steamextra/tier1/tsmultimempool.cpp")
target_link_libraries(gcsdk_game PRIVATE source_platform tier0 tier1 tier2 vstdlib source_messages)
# Local public and steamextra headers precede the vendor fallbacks.
target_include_directories(gcsdk_game BEFORE PRIVATE "${SOURCE_ROOT}/public" "${SOURCE_ROOT}/common" "${SOURCE_ROOT}/public/tier0" "${SOURCE_ROOT}/public/tier1" "${SOURCE_ROOT}/gcsdk/steamextra"
 "${SOURCE_ROOT}/public/gcsdk" "${M3_VENDOR}/public" "${M3_VENDOR}/public/gcsdk" "${M3_VENDOR}/gcsdk" "${M3_VENDOR}/gcsdk/steamextra")
target_compile_definitions(gcsdk_game PRIVATE GCSDK_LIB NO_STEAM_GAMECOORDINATOR)
target_compile_options(gcsdk_game PRIVATE -Wno-deprecated-declarations -Wno-inconsistent-missing-override)
add_dependencies(gcsdk_game source_messages)
target_link_libraries(server PRIVATE gcsdk_game)
target_link_libraries(matchmaking_ds PRIVATE gcsdk_game)
