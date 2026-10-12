set(PRIVATE_INVENTORY_DIR "${CMAKE_CURRENT_LIST_DIR}/private-inventory")
set(PRIVATE_INVENTORY_SOURCE "${CMAKE_BINARY_DIR}/private-inventory-source")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/prepare_inventory.py"
  --source-root "${SOURCE_ROOT}" --output "${PRIVATE_INVENTORY_SOURCE}"
  COMMAND_ERROR_IS_FATAL ANY)
get_target_property(private_server_sources server SOURCES)
foreach(relative game/server/cstrike15/cs_player.cpp game/server/gameinterface.cpp game/shared/econ/econ_entity.cpp
    game/shared/econ/econ_item_view.cpp game/shared/cstrike15/weapon_csbase.cpp game/shared/cstrike15/cs_gamerules.cpp)
  list(REMOVE_ITEM private_server_sources "${SOURCE_ROOT}/${relative}")
  list(APPEND private_server_sources "${PRIVATE_INVENTORY_SOURCE}/${relative}")
  get_filename_component(original_directory "${SOURCE_ROOT}/${relative}" DIRECTORY)
  set_source_files_properties("${PRIVATE_INVENTORY_SOURCE}/${relative}" PROPERTIES COMPILE_OPTIONS "-iquote;${original_directory};-I${PRIVATE_INVENTORY_DIR}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOURCE_ROOT}/${relative}")
endforeach()
set_property(TARGET server PROPERTY SOURCES "${private_server_sources}")
target_sources(server PRIVATE "${PRIVATE_INVENTORY_DIR}/network_inventory_server.cpp")
# Header methods are inline. Every server translation unit must see the same
# schema-based silencer capability, including original weapon/player sources.
target_compile_options(server PRIVATE "-ivfsoverlay" "${PRIVATE_INVENTORY_SOURCE}/weapon-header-overlay.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOURCE_ROOT}/game/shared/cstrike15/weapon_csbase.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools/prepare_inventory.py")
