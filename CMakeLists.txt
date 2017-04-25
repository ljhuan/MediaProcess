cmake_minimum_required (VERSION 3.3)
project (MP)

aux_source_directory(src Src)
include_directories(inc)
link_directories(lib/boost)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++11)
add_executable(mpserver ${Src})
target_link_libraries(mpserver
					pthread
					boost_filesystem
					boost_system
					boost_serialization
					boost_thread
					)
