if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
  list(APPEND CMAKE_EXE_LINKER_FLAGS_INIT "-lc++")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "/W[0-4]+" "" CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT}")
  string(REGEX REPLACE "/Zi" "" CMAKE_CXX_FLAGS_DEBUG_INIT "${CMAKE_CXX_FLAGS_DEBUG_INIT}")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "/W[0-4]+" "" CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
  string(REGEX REPLACE "/Zi" "" CMAKE_C_FLAGS_DEBUG_INIT "${CMAKE_C_FLAGS_DEBUG_INIT}")
endif()

