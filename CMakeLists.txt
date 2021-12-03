set(CMAKE_OSX_ARCHITECTURES "arm64")

cmake_minimum_required(VERSION 3.16)

# set the project name
project(NotOkImageFormat)

# add the executable
add_executable(noi
test/main.c
test/impl.c
noi_image.h
stb/stb_image.h
stb/stb_image_write.h
)
target_compile_definitions(noi PUBLIC NOI_THREADS)
target_include_directories(noi PUBLIC ${CMAKE_SOURCE_DIR})

if(UNIX AND NOT APPLE)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(noi PRIVATE Threads::Threads m)
endif()
