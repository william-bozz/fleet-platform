cmake_minimum_required(VERSION 3.16)
project(fleet_platform C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(fleetd
  src/main.c
  src/db.c
  src/http.c
  src/trucks.c
  src/trailers.c
  src/drivers.c
  src/loads.c
  vendor/civetweb/src/civetweb.c
  vendor/cJSON/cJSON.c
)

target_include_directories(fleetd PRIVATE
  src
  vendor/civetweb/include
  vendor/cJSON
)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

target_compile_definitions(fleetd PRIVATE OPENSSL_API_3_0 NO_SSL_DL)

target_link_libraries(fleetd PRIVATE
  Threads::Threads
  sqlite3
  dl
  OpenSSL::SSL
  OpenSSL::Crypto
)
