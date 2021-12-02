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
target_include_directories(noi PUBLIC
    ${CMAKE_SOURCE_DIR}
)