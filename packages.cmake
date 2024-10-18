include(FetchContent)

cmake_policy(PUSH)
cmake_policy(SET CMP0150 NEW)

set(FMT_INSTALL ON)

FetchContent_Declare(ext
    GIT_REPOSITORY ../libext.git
    GIT_TAG        76265c1325028676ae3219505bb362a0b28ad1ea # 0.3.0
)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        a33701196adfad74917046096bf5a2aa0ab0bb50 # 9.1.0
)

FetchContent_Declare(timber
    GIT_REPOSITORY ../timber.git
    GIT_TAG        9e6fd332fc3dc80a14ad8d5476a268ea867714f0 # 0.4.0
)

FetchContent_MakeAvailable(ext fmt timber)

if(PROJECT_TESTING)
    FetchContent_Declare(GTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        b10fad38c4026a29ea6561ab15fc4818170d1c10 # branch: main
    )

    FetchContent_MakeAvailable(GTest)
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(OPENSSL REQUIRED openssl>=3)

cmake_policy(POP)
