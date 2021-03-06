if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  get_filename_component(_clang_bin ${CMAKE_CXX_COMPILER} DIRECTORY)
  get_filename_component(_clang_lib ${_clang_bin}/../lib REALPATH)

  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-rpath,${_clang_lib} -lc++")

  unset(_clang_bin)
  unset(_clang_lib)

  if(USE_ASAN)
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fno-omit-frame-pointer -fsanitize=address")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "/W[0-4]+" "" CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT}")
  string(REGEX REPLACE "/Zi" "" CMAKE_CXX_FLAGS_DEBUG_INIT "${CMAKE_CXX_FLAGS_DEBUG_INIT}")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "/W[0-4]+" "" CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
  string(REGEX REPLACE "/Zi" "" CMAKE_C_FLAGS_DEBUG_INIT "${CMAKE_C_FLAGS_DEBUG_INIT}")
endif()

