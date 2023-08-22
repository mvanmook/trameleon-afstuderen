# -----------------------------------------------------------------

macro(tra_add_library)
  
  set(options "")
  set(one_value_args NAME)
  set(multi_value_args SOURCES DEPS)
  
  cmake_parse_arguments(
    TRA_LIB
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
    )

  if (NOT TRA_LIB_NAME)
    message(FATAL_ERROR "Missing the `NAME` argument.")
  endif()

  message("Creating library for Trameleon")

  if (TRA_BUILD_STATIC_LIB)

    # Static build
    add_library(${TRA_LIB_NAME} STATIC ${TRA_LIB_SOURCES})
    
  else()

    # Shared build
    add_library(${TRA_LIB_NAME} SHARED ${TRA_LIB_SOURCES})

    if (WIN32)
      install(TARGETS ${TRA_LIB_NAME} DESTINATION bin)
    else()
      install(TARGETS ${TRA_LIB_NAME} DESTINATION lib)
    endif()

    list(APPEND tra_libs ${TRA_LIB_NAME})

    # Make sure the lib DLL functions are exported, see api.h
    target_compile_definitions(${TRA_LIB_NAME} PRIVATE TRA_LIB_EXPORT)

    # Make sure we can find the shared morph library 
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
    
  endif()

  if (TRA_LIB_DEPS)
    add_dependencies(${TRA_LIB_NAME} ${TRA_LIB_DEPS})
  endif()
 

endmacro(tra_add_library)

# -----------------------------------------------------------------

macro(tra_add_module)
  
  set(options "")
  set(one_value_args NAME)
  set(multi_value_args SOURCES LIBS DEPS PRIVATE_INC_DIRS)
  
  cmake_parse_arguments(
    TRA_MODULE
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
    )

  if (NOT TRA_MODULE_NAME)
    message(FATAL_ERROR "Missing the `NAME` argument for the module.")
  endif()

  set(mod_name "tra-${TRA_MODULE_NAME}")

  message("Creating module `${TRA_MODULE_NAME}` for Trameleon")

  if (TRA_BUILD_STATIC_LIB)

    # Static build
    add_library(${mod_name} STATIC ${TRA_MODULE_SOURCES})
    
  else()

    # Shared build
    add_library(${mod_name} MODULE ${TRA_MODULE_SOURCES})
    install(TARGETS ${mod_name} DESTINATION lib)

    # Make sure the lib DLL functions are imported, see api.h
    target_compile_definitions(${mod_name} PRIVATE TRA_LIB_IMPORT) # Import the tra.lib
    target_compile_definitions(${mod_name} PRIVATE TRA_MODULE_EXPORT) # Export the `tra_load()` function. 

  endif()

  if (tra_deps)
    add_dependencies(${mod_name} ${tra_deps})
  endif()
  
  if (TRA_MODULE_DEPS)
    add_dependencies(${mod_name} ${TRA_MODULE_DEPS})
  endif()

  # Link libraries
  if (TRA_MODULE_LIBS)
    target_link_libraries(${mod_name} ${TRA_MODULE_LIBS})
  endif()

  # Add private include directories for this module only.
  if (TRA_MODULE_PRIVATE_INC_DIRS)
    target_include_directories(${mod_name} PRIVATE ${TRA_MODULE_PRIVATE_INC_DIRS})
  endif()
  

endmacro(tra_add_module)

# -----------------------------------------------------------------

macro(tra_create_test)

  set(options "")
  set(one_value_args NAME)
  set(multi_value_args SOURCES LABELS LIBS)

  cmake_parse_arguments(
    APP
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
    )

  if (NOT APP_NAME)
    message(FATAL_ERROR "Missing the `NAME` argument.")
  endif()

  # Collect the sources for this test
  set(app_source "${src_dir}/test/test-${APP_NAME}.c")

  # Create name, executable and test.
  set(app_name "test-${APP_NAME}${debug_flag}")
  add_executable(${app_name} ${app_source})
  target_link_libraries(${app_name} ${tra_libs})
  install(TARGETS ${app_name} DESTINATION bin)

  if (tra_deps)
    add_dependencies(${app_name} ${tra_deps})
  endif()

  if (tra_libs)
    target_link_libraries(${app_name} ${tra_libs})
  endif()
 
  # Link libraries
  if (APP_LIBS)
    target_link_libraries(${app_name} ${APP_LIBS})
  endif()


endmacro()

# -----------------------------------------------------------------
