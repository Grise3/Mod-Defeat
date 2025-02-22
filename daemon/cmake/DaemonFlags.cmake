# Daemon BSD Source Code
# Copyright (c) 2013-2016, Daemon Developers
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of the <organization> nor the
#    names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

add_definitions(-DDAEMON_BUILD_${CMAKE_BUILD_TYPE})

# Set flag without checking, optional argument specifies build type
macro(set_c_flag FLAG)
    if (${ARGC} GREATER 1)
        set(CMAKE_C_FLAGS_${ARGV1} "${CMAKE_C_FLAGS_${ARGV1}} ${FLAG}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
    endif()
endmacro()
macro(set_cxx_flag FLAG)
    if (${ARGC} GREATER 1)
        set(CMAKE_CXX_FLAGS_${ARGV1} "${CMAKE_CXX_FLAGS_${ARGV1}} ${FLAG}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}")
    endif()
endmacro()
macro(set_c_cxx_flag FLAG)
    set_c_flag(${FLAG} ${ARGN})
    set_cxx_flag(${FLAG} ${ARGN})
endmacro()
macro(set_linker_flag FLAG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FLAG}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FLAG}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${FLAG}")
endmacro()

function(try_flag LIST FLAG)
    string(REGEX REPLACE "[/=-]" "_" TEST ${FLAG})
    # Check if the flag begins with '/', meaning it is for MSVC.
    # check_CXX_compiler_flag apparently always fails for MSVC, so accept it without testing.
    # Other compilers might interpret it as a filename so reject without testing.
    string(SUBSTRING "${FLAG}" 0 1 FLAG_FIRST_CHAR)
    if ("${FLAG_FIRST_CHAR}" STREQUAL "/")
        if (MSVC)
            set(${TEST} 1)
        else()
            set(${TEST} 0)
        endif()
    else()
        set(CMAKE_REQUIRED_FLAGS "-Werror")
        check_CXX_compiler_flag("${FLAG}" ${TEST})
        set(CMAKE_REQUIRED_FLAGS "")
    endif()
    if (${TEST})
        set(${LIST} ${${LIST}} ${FLAG} PARENT_SCOPE)
    endif()
endfunction()

# Try flag and set if it works, optional argument specifies build type
macro(try_cxx_flag PROP FLAG)
    check_CXX_compiler_flag(${FLAG} FLAG_${PROP})
    if (FLAG_${PROP})
        set_cxx_flag(${FLAG} ${ARGV2})
    endif()
endmacro()
macro(try_c_cxx_flag PROP FLAG)
    # Only try the flag once on the C++ compiler
    try_cxx_flag(${PROP} ${FLAG} ${ARGV2})
    if (FLAG_${PROP})
        set_c_flag(${FLAG} ${ARGV2})
    endif()
endmacro()
# Clang prints a warning when if it doesn't support a flag, so use -Werror to detect
macro(try_cxx_flag_werror PROP FLAG)
    set(CMAKE_REQUIRED_FLAGS "-Werror")
    check_CXX_compiler_flag(${FLAG} FLAG_${PROP})
    set(CMAKE_REQUIRED_FLAGS "")
    if (FLAG_${PROP})
        set_cxx_flag(${FLAG} ${ARGV2})
    endif()
endmacro()
macro(try_c_cxx_flag_werror PROP FLAG)
    try_cxx_flag_werror(${PROP} ${FLAG} ${ARGV2})
    if (FLAG_${PROP})
        set_c_flag(${FLAG} ${ARGV2})
    endif()
endmacro()

macro(try_linker_flag PROP FLAG)
    # Check it with the C compiler
    set(CMAKE_REQUIRED_FLAGS ${FLAG})
    check_C_compiler_flag(${FLAG} FLAG_${PROP})
    set(CMAKE_REQUIRED_FLAGS "")
    if (FLAG_${PROP})
        set_linker_flag(${FLAG} ${ARGN})
    endif()
endmacro()

if(MINGW AND USE_BREAKPAD)
    set_linker_flag("-Wl,--build-id")
endif()

if (MSVC)
    set_c_cxx_flag("/MP")
    set_c_cxx_flag("/fp:fast")
    set_c_cxx_flag("/d2Zi+" RELWITHDEBINFO)

    # At least Ninja doesn't remove the /W3 flag when we add /W4|/Wall one, which
    # leads to compilation warnings.  Remove /W3 entirely, as /W4|/Wall be used.
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if (${flag_var} MATCHES "/W3")
        string(REGEX REPLACE "/W3" "" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()

    set_c_cxx_flag("/W4")

    if (ARCH STREQUAL "i686")
        set_c_cxx_flag("/arch:SSE2")
    endif()

    if (USE_LTO)
        set_c_cxx_flag("/GL" MINSIZEREL)
        set_c_cxx_flag("/GL" RELWITHDEBINFO)
        set_c_cxx_flag("/GL" RELEASE)
        set_linker_flag("/LTCG" MINSIZEREL)
        set_linker_flag("/LTCG" RELWITHDEBINFO)
        set_linker_flag("/LTCG" RELEASE)
    endif()
    set_linker_flag("/LARGEADDRESSAWARE")

    try_flag(WARNINGS   "/wd4068")

    # Turn off C4503:, e.g:
    # warning C4503: 'std::_Tree<std::_Tmap_traits<_Kty,_Ty,_Pr,_Alloc,false>>::_Insert_hint' : decorated name length exceeded, name was truncated
    # No issue will be caused from this error as long as no two symbols become identical by being truncated.
    # In practice this rarely happens and even the standard libraries are affected as in the example. So there really is not
    # much that can to done about it and the warnings about each truncation really just make it more likely
    # that other more real issues might get missed. So better to remove the distraction when it really is very unlikey to happen.
    set_c_cxx_flag("/wd4503")

    # Turn off warning C4996:, e.g:
    # warning C4996: 'open': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _open. See online help for details.    set_c_cxx_flag("/wd4996")
    # open seems far more popular than _open not to mention nicer. There doesn't seem to be any reason or will to change to _open.
    # So until there is a specific plan to tackle all of these type of warnings it's best to turn them off to the distraction.
    set_c_cxx_flag("/wd4996")
elseif (NACL)
    set_c_flag("-std=c11")
    set_cxx_flag("-std=gnu++14")

    set_c_cxx_flag("-ffast-math")
    set_c_cxx_flag("-fvisibility=hidden")
    set_c_cxx_flag("-stdlib=libc++")
    set_c_cxx_flag("--pnacl-allow-exceptions")

    set_c_cxx_flag("-Os -DNDEBUG"       MINSIZEREL)
    set_c_cxx_flag("-O3 -DNDEBUG"       RELEASE)
    set_c_cxx_flag("-O2 -DNDEBUG -g3"   RELWITHDEBINFO)
    set_c_cxx_flag("-O0          -g3"   DEBUG)
else()
    set_c_cxx_flag("-ffast-math")
    set_c_cxx_flag("-fno-strict-aliasing")

    # Among the required hardware features, the NX bit (No eXecute bit)
    # feature may be required for NativeClient to work. Some early
    # Intel EM64T processors are known to not implement the NX bit.
    # Some ARM CPUs may not implement the similar “execute never” feature.
    # While this is an hardware feature, this isn't a build option because it's
    # not used by the engine itself but by the NaCl loader. The engine will be
    # built but the game may or may not run. The NX bit feature may also be
    # disabled in the BIOS while the hardware supports it, so even if we would
    # detect the lack of it such knowledge would be useless at build time.
    # Running a server with a native executable game is also a valid usage
    # not requiring the NX bit.

    if (ARCH STREQUAL "amd64")
        # K8 or EM64T minimum: AMD Athlon 64 ClawHammer, Intel Xeon Nocona, Intel Pentium 4 model F (Prescott revision EO), VIA Nano.
        set(GCC_GENERIC_ARCH "x86-64")
        set(GCC_GENERIC_TUNE "generic")
    elseif (ARCH STREQUAL "i686")
        # P6 or K6 minimum: Intel Pentium Pro, AMD K6, Via Cyrix III, Via C3.
        set(GCC_GENERIC_ARCH "i686")
        set(GCC_GENERIC_TUNE "generic")
    elseif (ARCH STREQUAL "arm64")
        # Armv8-A minimum: Cortex-A50.
        set(GCC_GENERIC_ARCH "armv8-a")
        set(GCC_GENERIC_TUNE "generic")
    elseif (ARCH STREQUAL "armhf")
        # Armv7-A with VFP minimum: Cortex-A5.
        # Hard float ABI (mainstream 32-bit ARM Linux distributions).
        set(GCC_GENERIC_ARCH "armv7-a")
        set(GCC_GENERIC_TUNE "generic-armv7-a")
    elseif (ARCH STREQUAL "armel")
        # Armv6 minimum, optional VFP: ARM11.
        # Soft float ABI (mainstream 32-bit ARM Android distributions).
        set(GCC_GENERIC_ARCH "armv6")
        set(GCC_GENERIC_TUNE "generic")
    else()
        message(FATAL_ERROR "Unsupported architecture ${ARCH}")
    endif()

    option(USE_CPU_GENERIC_ARCHITECTURE "Enforce generic -march and -mtune compiler options" ON)
    if (USE_CPU_GENERIC_ARCHITECTURE)
        set_c_cxx_flag("-march=${GCC_GENERIC_ARCH}")
        set_c_cxx_flag("-mtune=${GCC_GENERIC_TUNE}")
    endif()

    option(USE_CPU_RECOMMENDED_FEATURES "Enforce usage of hardware features like SSE, NEON, VFP, MCX16, etc." ON)
    if (USE_CPU_RECOMMENDED_FEATURES)
        if (ARCH STREQUAL "amd64")
            # CMPXCHG16B minimum (x86-64-v2): AMD64 revision F.
            try_c_cxx_flag_werror(MCX16 "-mcx16")
        elseif (ARCH STREQUAL "i686")
            # SSE2 minimum: Intel Pentium 4 (Prescott), Intel Pentium M (Banias), AMD K8, Via C7.
            set_c_cxx_flag("-msse2")
            try_c_cxx_flag_werror(MFPMATH_SSE "-mfpmath=sse")
        elseif (ARCH STREQUAL "armhf")
            # NEON minimum.
            set_c_cxx_flag("-mfpu=neon")
        elseif (ARCH STREQUAL "armel")
            # VFP minimum, hardware float with soft float ABI
            set_c_cxx_flag("-mfloat-abi=softfp")
        endif()
    endif()

    # Use hidden symbol visibility if possible
    try_c_cxx_flag(FVISIBILITY_HIDDEN "-fvisibility=hidden")

    # Prevent the generation of STB_GNU_UNIQUE symbols on templated functions on Linux.
    # STB_GNU_UNIQUE renders dlclose() inoperative.
    try_cxx_flag(FNO_GNU_UNIQUE "-fno-gnu-unique")

    # Extra debug flags
    set_c_cxx_flag("-g3" DEBUG)
    set_c_cxx_flag("-g3" RELWITHDEBINFO)
    if (USE_DEBUG_OPTIMIZE)
        try_c_cxx_flag(OPTIMIZE_DEBUG "-Og" DEBUG)
    endif()

    # C++14 support
    try_cxx_flag(CXX14 "-std=c++14")
    if (NOT FLAG_CXX14)
        try_cxx_flag(CXX1Y "-std=c++1y")
        if (NOT FLAG_CXX1Y)
            try_cxx_flag(GNUXX14 "-std=gnu++14")
            if (NOT FLAG_GNUXX14)
                try_cxx_flag(GNUXX1Y "-std=gnu++1y")
                if (NOT FLAG_GNUXX1Y)
                    message(FATAL_ERROR "C++14 not supported by compiler")
                endif()
            endif()
        endif()
    endif()

    # Use MSVC-compatible bitfield layout
    if (WIN32)
        set_c_cxx_flag("-mms-bitfields")
    endif()

    # Use libc++ on Mac because the shipped libstdc++ version is too old
    if (APPLE)
        set_c_cxx_flag("-stdlib=libc++")
        set_linker_flag("-stdlib=libc++")
    endif()

    # Hardening, don't set _FORTIFY_SOURCE in debug builds
    if (USE_HARDENING OR NOT MINGW)
        # MinGW with _FORTIFY_SOURCE and without -fstack-protector causes unsatisfied dependency on libssp
        # https://github.com/msys2/MINGW-packages/issues/5868
        set_c_cxx_flag("-D_FORTIFY_SOURCE=2" RELEASE)
        set_c_cxx_flag("-D_FORTIFY_SOURCE=2" RELWITHDEBINFO)
        set_c_cxx_flag("-D_FORTIFY_SOURCE=2" MINSIZEREL)
    endif()
    if (USE_HARDENING)
        try_c_cxx_flag(FSTACK_PROTECTOR_STRONG "-fstack-protector-strong")
        if (NOT FLAG_FSTACK_PROTECTOR_STRONG)
            try_c_cxx_flag(FSTACK_PROTECTOR_ALL "-fstack-protector-all")
        endif()
        try_c_cxx_flag(FNO_STRICT_OVERFLOW "-fno-strict-overflow")
        try_c_cxx_flag(WSTACK_PROTECTOR "-Wstack-protector")
        try_c_cxx_flag(FPIE "-fPIE")
        try_linker_flag(LINKER_PIE "-pie")
        if (${FLAG_LINKER_PIE} AND MINGW)
            # https://github.com/msys2/MINGW-packages/issues/4100
            if (ARCH STREQUAL "i686")
                set_linker_flag("-Wl,-e,_mainCRTStartup")
            elseif(ARCH STREQUAL "amd64")
                set_linker_flag("-Wl,-e,mainCRTStartup")
            else()
                message(FATAL_ERROR "Unsupported architecture ${ARCH}")
            endif()
        endif()
    endif()

    # Linker flags
    if (NOT APPLE)
        try_linker_flag(LINKER_O1 "-Wl,-O1")
        try_linker_flag(LINKER_SORT_COMMON "-Wl,--sort-common")
        try_linker_flag(LINKER_AS_NEEDED "-Wl,--as-needed")
        if (NOT USE_ADDRESS_SANITIZER)
            try_linker_flag(LINKER_NO_UNDEFINED "-Wl,--no-undefined")
        endif()
        try_linker_flag(LINKER_Z_RELRO "-Wl,-z,relro")
        try_linker_flag(LINKER_Z_NOW "-Wl,-z,now")
    endif()
    if (WIN32)
        try_linker_flag(LINKER_DYNAMICBASE "-Wl,--dynamicbase")
        try_linker_flag(LINKER_NXCOMPAT "-Wl,--nxcompat")
        try_linker_flag(LINKER_LARGE_ADDRESS_AWARE "-Wl,--large-address-aware")
        try_linker_flag(LINKER_HIGH_ENTROPY_VA "-Wl,--high-entropy-va")
    endif()

    # The -pthread flag sets some preprocessor defines,
    # it is also used to link with libpthread on Linux
    if (NOT APPLE)
        try_c_cxx_flag(PTHREAD "-pthread")
    endif()
    if (LINUX)
        set_linker_flag("-pthread")
    endif()

    # Warning options
    try_flag(WARNINGS           "-Wall")
    try_flag(WARNINGS           "-Wextra")
    if (USE_PEDANTIC)
        try_flag(WARNINGS       "-pedantic")
    endif()

    if (USE_ADDRESS_SANITIZER)
        set_cxx_flag("-fsanitize=address")
        set_linker_flag("-fsanitize=address")
    endif()

    # Link-time optimization
    if (USE_LTO)
        set_c_cxx_flag("-flto")
        set_linker_flag("-flto")

        # For LTO compilation we must send a copy of all compile flags to the linker
        set_linker_flag("${CMAKE_CXX_FLAGS}")

        # Use gcc-ar and gcc-ranlib instead of ar and ranlib so that we can use
        # slim LTO objects. This requires a recent version of GCC and binutils.
        if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
            if (USE_SLIM_LTO)
                string(REGEX MATCH "^([0-9]+.[0-9]+)" _version "${CMAKE_CXX_COMPILER_VERSION}")
                get_filename_component(COMPILER_BASENAME "${CMAKE_C_COMPILER}" NAME)
                if (COMPILER_BASENAME MATCHES "^(.+-)g?cc(-[0-9]+\\.[0-9]+\\.[0-9]+)?(\\.exe)?$")
                    set(TOOLCHAIN_PREFIX ${CMAKE_MATCH_1})
                endif()

                find_program(GCC_AR
                    NAMES
                        "${TOOLCHAIN_PREFIX}gcc-ar"
                        "${TOOLCHAIN_PREFIX}gcc-ar-${_version}"
                    DOC "gcc provided wrapper for ar which adds the --plugin option"
                )

                find_program(GCC_RANLIB
                    NAMES
                        "${TOOLCHAIN_PREFIX}gcc-ranlib"
                        "${TOOLCHAIN_PREFIX}gcc-ranlib-${_version}"
                    DOC "gcc provided wrapper for ranlib which adds the --plugin option"
                )

                mark_as_advanced(GCC_AR GCC_RANLIB)

                # Override standard ar and ranlib with the gcc- versions
                if (GCC_AR)
                    set(CMAKE_AR ${GCC_AR})
                endif()
                if (GCC_RANLIB)
                    set(CMAKE_RANLIB ${GCC_RANLIB})
                endif()

                try_c_cxx_flag(NO_FAT_LTO_OBJECTS "-fno-fat-lto-objects")
            else()
                try_c_cxx_flag(FAT_LTO_OBJECTS "-ffat-lto-objects")
            endif()
        endif()
    endif()

endif()

if (USE_WERROR)
    try_flag(WARNINGS "-Werror")
    try_flag(WARNINGS "/WX")
    if (USE_PEDANTIC)
        try_flag(WARNINGS "-pedantic-errors")
    endif()
endif()

# Windows-specific definitions
if (WIN32)
    add_definitions(
        -DWINVER=0x501  # Minimum Windows version: XP
        -DWIN32         # Define WIN32 for compatibility (compiler defines _WIN32)
        -DNOMINMAX      # Define NOMINMAX to prevent conflics between std::min/max and the min/max macros in WinDef.h
        -DSTRICT        # Enable STRICT type checking for windows.h
    )
    set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_FIND_LIBRARY_PREFIXES} "" "lib")
endif()
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Mac-specific definitions
if (APPLE)
    add_definitions(-DMACOS_X)
    set(CMAKE_INSTALL_RPATH "@executable_path")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

# Configuration specific definitions

# This stupid trick to define THIS_IS_NOT_A_DEBUG_BUILD (rather than nothing) in the non-debug case
# is so that it doesn't break the hacky gcc/clang PCH code which reads all the definitions
# and prefixes "-D" to them.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
             $<$<NOT:$<CONFIG:Debug>>:THIS_IS_NOT_A_>DEBUG_BUILD)
