cmake_minimum_required(VERSION 3.15)
project(guess_ipc C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(game_signals src/game_signals.c)
add_executable(game_pipe    src/game_pipe.c)
