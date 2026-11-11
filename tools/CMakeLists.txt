cmake_minimum_required(VERSION 3.20)
project(ndv_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED OPENSSL_ROOT_DIR AND WIN32 AND EXISTS "C:/Program Files/OpenSSL-Win64")
  set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
endif()
find_package(OpenSSL REQUIRED)

add_executable(reseed_raven reseed_raven.cpp)
target_include_directories(reseed_raven PRIVATE ../server/src/common)
target_link_libraries(reseed_raven PRIVATE OpenSSL::Crypto)
