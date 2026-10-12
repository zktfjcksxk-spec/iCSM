# Valve GNS 65f529731bcfcb618712eecf2e93dd41b549d0dc, original GNS_SRCS.
set(GNS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vendor/m3/gns")
if(NOT OPENSSL_ROOT_DIR)
  set(OPENSSL_ROOT_DIR "/usr")
endif()
find_package(OpenSSL REQUIRED)
set(GNS_FILES
 "${GNS_ROOT}/src/external/curve25519-donna/curve25519.c"
 "${GNS_ROOT}/src/external/ed25519-donna/ed25519_VALVE.c"
 "${GNS_ROOT}/src/common/crypto.cpp"
 "${GNS_ROOT}/src/common/opensslwrapper.cpp"
 "${GNS_ROOT}/src/common/steamid.cpp"
 "${GNS_ROOT}/src/public/minbase/minbase_common_errors.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/certtool/steamnetworkingsockets_certtool.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/csteamnetworkingsockets.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/steamnetworkingsockets_flat.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/steamnetworkingsockets_connections.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/steamnetworkingsockets_lowlevel.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/steamnetworkingsockets_snp.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/clientlib/steamnetworkingsockets_udp.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/steamnetworkingsockets_certs.cpp"
 "${GNS_ROOT}/src/steamnetworkingsockets/steamnetworkingsockets_shared.cpp"
 "${GNS_ROOT}/src/tier0/cpu.cpp"
 "${GNS_ROOT}/src/tier0/dbg.cpp"
 "${GNS_ROOT}/src/tier0/platformtime.cpp"
 "${GNS_ROOT}/src/tier1/netadr.cpp"
 "${GNS_ROOT}/src/tier1/utlbuffer.cpp"
 "${GNS_ROOT}/src/tier1/utlmemory.cpp"
 "${GNS_ROOT}/src/vstdlib/strtools.cpp"
)
set(GNS_PROTO_FILES steamnetworkingsockets_messages_certs steamnetworkingsockets_messages steamnetworkingsockets_messages_udp)
foreach(proto IN LISTS GNS_PROTO_FILES)

 list(APPEND GNS_FILES "${CMAKE_CURRENT_LIST_DIR}/../generated/${proto}.pb.cc")
endforeach()
add_library(GameNetworkingSockets SHARED ${GNS_FILES})
target_include_directories(GameNetworkingSockets PRIVATE "${GNS_ROOT}/include" "${GNS_ROOT}/src/common"
 "${GNS_ROOT}/src/public" "${CMAKE_CURRENT_LIST_DIR}/../generated")
target_compile_definitions(GameNetworkingSockets PRIVATE STEAMDATAGRAMLIB_FOREXPORT STATIC_TIER0 ENABLE_CRYPTO_25519
 HAVE_OPENSSL ENABLE_OPENSSLCONNECTION CRYPTO_DISABLE_ENCRYPT_WITH_PASSWORD GOOGLE_PROTOBUF_NO_RTTI POSIX LINUX GNUC GNU_COMPILER)
target_compile_options(GameNetworkingSockets PRIVATE -fvisibility=hidden -fno-strict-aliasing -Wno-deprecated-declarations
 "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>" "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>")
target_link_libraries(GameNetworkingSockets PRIVATE libprotobuf OpenSSL::Crypto)

# AArch64 builds the dependency's original generic curve/ed25519 backend.
