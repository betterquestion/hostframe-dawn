cmake_minimum_required(VERSION 3.22)
project(hostframe_dawn LANGUAGES CXX)

set(DAWN_INPUTS "" CACHE PATH "Dawn inputs prepared by bun setup")
set(dawn_source "${DAWN_INPUTS}/source")
set(dawn_generated "${DAWN_INPUTS}/generated")
set(dawn_headers "${DAWN_INPUTS}/headers/include")
set(dawn_archive "${DAWN_INPUTS}/engine/lib64/libwebgpu_dawn.a")
set(node_source "${dawn_source}/src/dawn/node")

if(NOT EXISTS "${dawn_archive}")
  message(FATAL_ERROR "Dawn engine archive is missing. Run bun setup.")
endif()

find_package(Threads REQUIRED)
find_package(Git REQUIRED)
add_library(dawn_engine STATIC IMPORTED)
set_target_properties(dawn_engine PROPERTIES IMPORTED_LOCATION "${dawn_archive}")

# Apply the direct-link and instance-lifetime fixes to a build-local copy.
set(binding_patch "${CMAKE_CURRENT_SOURCE_DIR}/patches/dawn-node.patch")
set(patched_files
  src/dawn/node/Module.cpp
  src/dawn/node/binding/AsyncRunner.cpp
  src/dawn/node/binding/AsyncRunner.h
)
set(patch_stage "${CMAKE_CURRENT_BINARY_DIR}/patch-stage")
set(patched "${CMAKE_CURRENT_BINARY_DIR}/patched")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${binding_patch}")
foreach(path IN LISTS patched_files)
  configure_file("${dawn_source}/${path}" "${patch_stage}/${path}" COPYONLY)
endforeach()
execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --no-index "${binding_patch}"
  WORKING_DIRECTORY "${patch_stage}"
  COMMAND_ERROR_IS_FATAL ANY
)
file(MAKE_DIRECTORY "${patched}/src/dawn/node/binding")
foreach(path IN LISTS patched_files)
  file(COPY_FILE "${patch_stage}/${path}" "${patched}/${path}" ONLY_IF_DIFFERENT)
endforeach()

file(GLOB binding_sources CONFIGURE_DEPENDS "${node_source}/binding/*.cpp")
list(REMOVE_ITEM binding_sources "${node_source}/binding/AsyncRunner.cpp")
add_library(dawn MODULE
  src/bridge.cpp
  "${patched}/src/dawn/node/Module.cpp"
  "${patched}/src/dawn/node/binding/AsyncRunner.cpp"
  "${node_source}/ManualConstructors.cpp"
  "${node_source}/interop/Core.cpp"
  "${dawn_generated}/src/dawn/node/interop/WebGPU.cpp"
  "${dawn_generated}/src/dawn/utils/ComboLimits.cpp"
  ${binding_sources}
)
set_target_properties(dawn PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
target_include_directories(dawn PRIVATE
  include
  "${patched}"
  "${dawn_headers}"
  "${dawn_generated}"
  "${dawn_generated}/src"
  "${dawn_source}"
  "${dawn_source}/include"
)
target_include_directories(dawn SYSTEM PRIVATE
  "${dawn_source}/third_party/node-addon-api"
  "${dawn_source}/third_party/node-api-headers/include"
)
target_compile_definitions(dawn PRIVATE DAWN_HAVE_UTILS_BUILD_CONFIG)
# The engine's private dependencies can also be present in the host runtime.
target_link_options(dawn PRIVATE "LINKER:--exclude-libs,ALL")
target_link_libraries(dawn PRIVATE dawn_engine Threads::Threads ${CMAKE_DL_LIBS})

add_library(dawn_abi_fixture MODULE EXCLUDE_FROM_ALL test/abi-fixture.cpp)
set_target_properties(dawn_abi_fixture PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
target_include_directories(dawn_abi_fixture PRIVATE include)
target_include_directories(dawn_abi_fixture SYSTEM PRIVATE
  "${dawn_headers}"
  "${dawn_source}/third_party/node-addon-api"
  "${dawn_source}/third_party/node-api-headers/include"
)
target_link_libraries(dawn_abi_fixture PRIVATE ${CMAKE_DL_LIBS})

find_package(X11 QUIET)
if(X11_FOUND)
  target_sources(dawn_abi_fixture PRIVATE test/x11-surface.cpp)
  target_compile_definitions(dawn_abi_fixture PRIVATE HOSTFRAME_DAWN_X11)
  target_link_libraries(dawn_abi_fixture PRIVATE X11::X11 Threads::Threads)
endif()
