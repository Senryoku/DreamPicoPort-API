set(CMAKE_C_COMPILER zig cc -fno-sanitize=all)
set(CMAKE_CXX_COMPILER zig c++ -fno-sanitize=all)
set(CMAKE_C_COMPILER_TARGET ${TARGET})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET})

if(LINUX)
	set(CMAKE_CXX_FLAGS "-isystem /usr/include")
	set(CMAKE_C_FLAGS "-isystem /usr/include")
endif()