set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(physx_tutorial-muxer
  muxer.cc
)

find_package(Boost REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AETHER_COMMON REQUIRED aether-common)
pkg_check_modules(AETHER_NETCODE REQUIRED aether-generic-netcode)


target_compile_definitions(physx_tutorial-muxer PRIVATE WITH_HADEAN_LOGGING)

target_include_directories(physx_tutorial-muxer
  PRIVATE ../
  PRIVATE ${BOOST_INCLUDE_DIRS}
  PRIVATE ${AETHER_COMMON_INCLUDE_DIRS}
  PRIVATE ${AETHER_NETCODE_INCLUDE_DIRS}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(physx_tutorial-muxer
  PRIVATE -static
  PRIVATE -Wl,--start-group
  PRIVATE ${BOOST_LDFLAGS}
  PRIVATE ${AETHER_NETCODE_LDFLAGS}
  PRIVATE ${AETHER_COMMON_LDFLAGS}
)
