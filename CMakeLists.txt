cmake_minimum_required(VERSION 3.26)

include(cmake/modules.cmake)
include(GitVersion)

project(netcore
    VERSION ${GIT_VERSION_ABBREV}
    DESCRIPTION "Networking library for C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

include(ProjectTesting)
include(packages.cmake)

add_library(netcore "")
add_library(netcore::netcore ALIAS netcore)

target_sources(netcore PUBLIC FILE_SET HEADERS BASE_DIRS include)
target_include_directories(netcore PUBLIC ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(netcore
    PUBLIC
        ext::ext
        fmt::fmt
        timber::timber
        ${OPENSSL_LIBRARIES}
)

if(PROJECT_TESTING)
    add_executable(netcore.test "")

    target_link_libraries(netcore.test PRIVATE
        GTest::gtest
        netcore
    )

    add_test(
        NAME "Unit Tests"
        COMMAND netcore.test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
endif()

add_subdirectory(include)
add_subdirectory(src)

include(Sanitizers)
enable_asan()

include(EasyInstall)
easy_install(
    TARGETS
        netcore
    DEPENDENCIES
        ext
        fmt
        timber
)

if(PROJECT_IS_TOP_LEVEL)
    add_custom_target(docker
        docker build --progress=plain --no-cache .
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()
