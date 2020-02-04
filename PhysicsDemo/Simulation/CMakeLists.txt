cmake_minimum_required(VERSION 3.10)

project(physx_tutorial)

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
SET(BUILD_SHARED_LIBS OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")

add_subdirectory(physx_tutorial-muxer)


set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DEBUG -Wno-error -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-unused-function -Wno-c99-extensions -mbmi2 -fPIC")
set(PHYSX_FLAGS "-Wl,--start-group -lPhysXCharacterKinematic_static_64 -lPhysXCommon_static_64 -lPhysXCooking_static_64 -lPhysXExtensions_static_64 -lPhysXFoundation_static_64 -lPhysXPvdSDK_static_64 -lPhysX_static_64 -lPhysXVehicle_static_64 -Wl,--end-group")
# Load flags from pkg-config to link/compile with Aether
find_package(PkgConfig REQUIRED)
pkg_check_modules(AETHER REQUIRED aether)
pkg_check_modules(AETHER_DEMO_COMMON REQUIRED aether-demo-common)

# folly
find_package(folly REQUIRED)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)

# Create executable from sources
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cc)
add_executable(physx_tutorial ${SRC_FILES})

# Add protocol files to includes
target_include_directories(physx_tutorial PRIVATE ${PROJECT_SOURCE_DIR} PRIVATE)

# Link with Aether Libraries and fmt lib.
target_link_libraries(physx_tutorial aether-demo-common Folly::folly ${AETHER_STATIC_LDFLAGS} ${AETHER_STATIC_LDFLAGS_OTHER} stdc++fs ${PHYSX_FLAGS})

# Add compiler flags for includes
target_compile_options(physx_tutorial PUBLIC ${AETHER_STATIC_CFLAGS} ${AETHER_STATIC_CFLAGS_OTHER})
