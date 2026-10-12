# Source lists from protobuf 2.5.0 src/Makefile.am (fixed official commit).
set(PROTOBUF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vendor/m3/protobuf-2.5.0")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/protobuf-config")
configure_file("${CMAKE_CURRENT_LIST_DIR}/protobuf25-config.h.in" "${CMAKE_BINARY_DIR}/protobuf-config/config.h" COPYONLY)
add_library(libprotobuf STATIC
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/common.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/once.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/stringprintf.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/extension_set.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/generated_message_util.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/message_lite.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/repeated_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/wire_format_lite.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/coded_stream.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/zero_copy_stream.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/zero_copy_stream_impl_lite.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/strutil.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/substitute.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/stubs/structurally_valid.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/descriptor.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/descriptor.pb.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/descriptor_database.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/dynamic_message.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/extension_set_heavy.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/generated_message_reflection.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/message.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/reflection_ops.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/service.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/text_format.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/unknown_field_set.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/wire_format.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/gzip_stream.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/printer.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/tokenizer.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/io/zero_copy_stream_impl.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/importer.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/parser.cc"
)
target_include_directories(libprotobuf PUBLIC "${PROTOBUF_ROOT}/src" PRIVATE "${CMAKE_BINARY_DIR}/protobuf-config")
set_target_properties(libprotobuf PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(libprotobuf PRIVATE -Wno-deprecated-declarations)
add_library(libprotoc STATIC
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/code_generator.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/command_line_interface.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/plugin.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/plugin.pb.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/subprocess.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/zip_writer.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_enum.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_enum_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_extension.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_file.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_generator.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_helpers.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_message.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_message_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_service.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/cpp/cpp_string_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_enum.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_enum_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_extension.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_file.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_generator.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_helpers.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_message.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_message_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_primitive_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_service.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_string_field.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/java/java_doc_comment.cc"
 "${PROTOBUF_ROOT}/src/google/protobuf/compiler/python/python_generator.cc"
)
target_include_directories(libprotoc PUBLIC "${PROTOBUF_ROOT}/src" PRIVATE "${CMAKE_BINARY_DIR}/protobuf-config")
set_target_properties(libprotoc PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(libprotoc PRIVATE -Wno-deprecated-declarations)
target_link_libraries(libprotobuf PUBLIC z)
target_link_libraries(libprotoc PUBLIC libprotobuf)
add_executable(protoc "${PROTOBUF_ROOT}/src/google/protobuf/compiler/main.cc")
target_link_libraries(protoc PRIVATE libprotoc)
