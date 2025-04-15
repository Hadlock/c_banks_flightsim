cmake_minimum_required(VERSION 3.10)

project(BanksQuickdraw C)
set(CMAKE_C_STANDARD 99)

add_executable(banks banks.c)

target_link_libraries(banks
    m
    "-lInterfaceLib"
)