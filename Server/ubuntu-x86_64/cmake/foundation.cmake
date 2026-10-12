# All lists are selected from the local 2019 VPC POSIX branches.
function(source_library name type folder)
  set(files ${ARGN})
  list(TRANSFORM files PREPEND "${SOURCE_ROOT}/${folder}/")
  list(TRANSFORM files APPEND ".cpp")
  add_library(${name} ${type} ${files})
  target_link_libraries(${name} PUBLIC source_platform PRIVATE tier0)
  set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
source_library(interfaces STATIC interfaces interfaces)
source_library(tier1 STATIC tier1 appinstance bitbuf bitstring newbitbuf byteswap
  characterset checksum_crc checksum_md5 checksum_sha1 circularbuffer commandbuffer
  convar datamanager diff exprevaluator generichash floatprint interface keyvalues
  keyvaluesjson kvpacker lzmaDecoder lzss mempool memstack NetAdr splitstring
  processor_detect_linux rangecheckedvar stringpool strtools strtools_unicode tier1
  tier1_logging timeutils uniqueid utlbuffer utlbufferutil utlpointers utlsoacontainer
  utlstring utlstringtoken utlsymbol miniprofiler_hash sparsematrix memoverride_dummy)
target_compile_definitions(tier1 PRIVATE TIER1_STATIC_LIB)
target_link_libraries(tier1 PUBLIC )
target_sources(tier1 PRIVATE "${SOURCE_ROOT}/thirdparty/JSON_parser/JSON_parser.c")
source_library(mathlib STATIC mathlib expressioncalculator color_conversion cholesky
  halton lightdesc mathlib_base powsse sparse_convolution_noise sseconst sse ssenoise
  anorms bumpvects IceKey hexes kdop imagequant spherical polyhedron quantize randsse
  simdvectormatrix vmatrix almostequal simplex eigen box_buoyancy camera planefit
  polygon volumeculler transform sphere capsule)
target_compile_definitions(mathlib PRIVATE MATHLIB_LIB)
target_include_directories(mathlib PRIVATE "${SOURCE_ROOT}/public/mathlib")
source_library(vstdlib SHARED vstdlib coroutine cvar jobthread keyvaluessystem random
  vcover eventsystem vstrtools vstdlib_strtools)
target_compile_definitions(vstdlib PRIVATE VSTDLIB_DLL_EXPORT MEMOVERRIDE_MODULE=vstdlib)
target_compile_options(vstdlib PRIVATE -fno-stack-protector)
target_sources(vstdlib PRIVATE "${SOURCE_ROOT}/public/tier0/memoverride.cpp"
  "${SOURCE_ROOT}/public/tier0/crtoverride.cpp")
target_link_libraries(vstdlib PRIVATE tier1 mathlib interfaces )
source_library(tier2 STATIC tier2 beamsegdraw defaultfilesystem fileutils keybindings
  meshutils p4helpers renderutils riff soundutils tier2 tier2_logging tokenreader
  util_init utlstreambuffer vconfig schemaclassbinding socketcreator resourceprecacher interval)
target_sources(tier2 PRIVATE "${SOURCE_ROOT}/public/map_utils.cpp"
  "${SOURCE_ROOT}/public/materialsystem/MaterialSystemUtil.cpp")
target_include_directories(tier2 PRIVATE "${SOURCE_ROOT}/public/tier2")
source_library(vpklib STATIC vpklib packedstore)
source_library(filesystem_stdio SHARED filesystem basefilesystem basefilesystemasync
  filetracker filesystemasync filesystem_stdio QueuedLoader linux_support)
target_sources(filesystem_stdio PRIVATE "${SOURCE_ROOT}/public/kevvaluescompiler.cpp"
  "${SOURCE_ROOT}/public/zip_utils.cpp" "${SOURCE_ROOT}/public/tier0/memoverride.cpp"
  "${SOURCE_ROOT}/public/tier0/crtoverride.cpp")
target_compile_definitions(filesystem_stdio PRIVATE FILESYSTEM_STDIO_EXPORTS
  DONT_PROTECT_FILEIO_FUNCTIONS PROTECTED_THINGS_ENABLE MEMOVERRIDE_MODULE=filesystem_stdio)
target_link_libraries(filesystem_stdio PRIVATE tier2 vpklib vstdlib tier1 mathlib interfaces )
