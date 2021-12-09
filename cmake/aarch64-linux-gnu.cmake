set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(triple aarch64-linux-gnu)
set(DISABLE_UT ON)

set(CMAKE_STAGING_PREFIX /opt/externs/libs/${triple})

if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX .cross/${triple})
endif()

set(CMAKE_STAGING_PREFIX /opt/externs/libs/${triple})
set(CMAKE_PREFIX_PATH ${CMAKE_STAGING_PREFIX})

set(CMAKE_C_COMPILER ${triple}-gcc)
set(CMAKE_C_FLAGS
    "-O3 -fPIC"
    CACHE INTERNAL "")
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -g -Wall"
    CACHE INTERNAL "")
set(CMAKE_C_FLAGS_MINSIZEREL
    "${CMAKE_C_FLAGS} -Os -DNDEBUG"
    CACHE INTERNAL "")
set(CMAKE_C_FLAGS_RELEASE
    "${CMAKE_C_FLAGS} -O4 -DNDEBUG"
    CACHE INTERNAL "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO
    "${CMAKE_C_FLAGS} -O2 -g"
    CACHE INTERNAL "")

# set(CMAKE_CXX_COMPILER ${triple}-g++) set(CMAKE_CXX_FLAGS "-O3 -fPIC" CACHE
# INTERNAL "") set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -Wall" CACHE
# INTERNAL "") set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} -Os -DNDEBUG"
# CACHE INTERNAL "") set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O4
# -DNDEBUG" CACHE INTERNAL "") set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
# "${CMAKE_CXX_FLAGS} -O2 -g" CACHE INTERNAL "")
# include_directories(/opt/externs/libs/${triple}/include)
# include_directories(/opt/externs/libs/${triple}/openssl/include)

# set(CMAKE_C_COMPILER ${triple}-gcc) set(CMAKE_CXX_COMPILER ${triple}-g++)

set(CMAKE_AR ${triple}-ar)
set(CMAKE_AR ${triple}-ar)
set(CMAKE_LINKER ${triple}-ld)
set(CMAKE_NM ${triple}-nm)
set(CMAKE_OBJDUMP ${triple}-objdump)
set(CMAKE_RANLIB ${triple}-ranlib)

include_directories(SYSTEM ${CMAKE_STAGING_PREFIX}/include)
include_directories(SYSTEM ${CMAKE_STAGING_PREFIX}/openssl/include)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_STAGING_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_SYSROOT ${CMAKE_STAGING_PREFIX})
link_directories(${CMAKE_STAGING_PREFIX})
