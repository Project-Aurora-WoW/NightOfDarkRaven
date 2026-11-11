set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# OpenSSL (SRP6 / password hashing) — обязателен: SRP6.h включает openssl/* без условий.
if(NOT DEFINED OPENSSL_ROOT_DIR AND WIN32 AND EXISTS "C:/Program Files/OpenSSL-Win64")
  set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
endif()
find_package(OpenSSL REQUIRED)

# MySQL client (8.0)
if(WIN32 AND NOT DEFINED MySQL_INCLUDE_DIR)
  set(MySQL_INCLUDE_DIR "C:/Program Files/MySQL/MySQL Server 8.0/include" CACHE PATH "MySQL headers")
endif()
if(WIN32 AND NOT DEFINED MySQL_LIBRARY)
  set(MySQL_LIBRARY "C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.lib" CACHE FILEPATH "MySQL client library")
endif()

add_library(ndv_common INTERFACE)
target_include_directories(ndv_common INTERFACE src/common ${MySQL_INCLUDE_DIR})
target_link_libraries(ndv_common INTERFACE Threads::Threads OpenSSL::Crypto)
if(WIN32)
  target_link_libraries(ndv_common INTERFACE ws2_32 mswsock "${MySQL_LIBRARY}")
  target_compile_definitions(ndv_common INTERFACE _WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

foreach(server authserver worldserver registerserver)
  add_executable(${server} src/${server}/main.cpp)
  target_link_libraries(${server} PRIVATE ndv_common)
endforeach()
