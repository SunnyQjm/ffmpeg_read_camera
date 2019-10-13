cmake_minimum_required(VERSION 3.13)
project(ffmpeg_read_camera)

set(CMAKE_CXX_STANDARD 14)

add_executable(ffmpeg_read_camera main.cpp)

target_link_libraries(ffmpeg_read_camera avcodec avformat swscale avdevice avutil SDL2 SDL2_image sdl_helper)