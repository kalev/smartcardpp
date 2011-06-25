cmake_minimum_required(VERSION 2.6)
project(smartcardpp)

# Custom cmake modules
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(InstallSettings)

set(SMARTCARDPP_VERSION "3.1.0")

if(UNIX)
    find_package(PCSCLite REQUIRED)
endif(UNIX)

if(MSVC)
    add_definitions(
        # Release builds don't compile without this definition.
        # http://social.msdn.microsoft.com/Forums/en-US/vcplus2008prerelease/thread/fa969def-911e-4760-867d-5a06a37ed048
        -D_SECURE_SCL=0
    )
    # disable incremental linking to work around a missing MSVCR90D.dll with
    # debug builds
    SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
    SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
    SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
endif(MSVC)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PCSCLITE_INCLUDE_DIR}
)

if(MSVC AND USE_STATIC_RUNTIME)
    message(STATUS "Building with static MSVC runtime")
    foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
endif(MSVC AND USE_STATIC_RUNTIME)

set(smartcard_SRCS
    CardBase.cpp
    DynamicLibrary.cpp
    locked_allocator.cpp
    PCSCManager.cpp
    SCError.cpp
    types.cpp
)

set(esteid_SRCS
    esteid/EstEidCard.cpp
)

# Public headers which get installed into smartcardpp/ directory
set(smartcard_HDRS
    CardBase.h
    common.h
    DynamicLibrary.h
    helperMacro.h
    internal-pcsc22.h
    locked_allocator.h
    ManagerInterface.h
    PCSCManager.h
    PinString.h
    SCError.h
    smartcardpp.h
    types.h
)

# Public headers which get installed into smartcardpp/esteid/ directory
set(esteid_HDRS
    esteid/EstEidCard.h
)


set(GENERATED_HEADERS "")
macro(stage_smartcard_headers HEADERS)
    foreach(H ${HEADERS})
        set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/${H}")
        set(DST "${CMAKE_CURRENT_BINARY_DIR}/smartcardpp/${H}")

        add_custom_command(
            OUTPUT ${DST}
            COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
            DEPENDS ${SRC}
        )

        set(GENERATED_HEADERS ${GENERATED_HEADERS} ${DST})
    endforeach(H)
endmacro(stage_smartcard_headers)

stage_smartcard_headers("${smartcard_HDRS}")
stage_smartcard_headers("${esteid_HDRS}")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/smartcardpp DESTINATION ${INCLUDE_INSTALL_DIR})

add_library(smartcardpp STATIC
    ${GENERATED_HEADERS}
    ${smartcard_SRCS}
    ${esteid_SRCS}
)

if(WIN32)
    set(WIN_LIBRARIES ws2_32)
endif(WIN32)

target_link_libraries(smartcardpp ${CMAKE_DL_LIBS} ${WIN_LIBRARIES})

set_target_properties(smartcardpp PROPERTIES VERSION ${SMARTCARDPP_VERSION} SOVERSION 0)
install(TARGETS smartcardpp DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

add_executable(card-test compat_getopt.c compat_getpass.c test.cpp)
target_link_libraries(card-test smartcardpp ${CMAKE_DL_LIBS})
install(TARGETS card-test DESTINATION ${BIN_INSTALL_DIR})
