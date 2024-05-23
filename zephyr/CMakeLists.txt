cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(stm32nucleo_zenoh)

FILE(GLOB app_sources ../src/*.c*)
# FILE(GLOB app_sources ../src/z_pub.c)
target_sources(app PRIVATE ${app_sources})
