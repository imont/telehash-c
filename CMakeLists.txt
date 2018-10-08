# Note: only used for CMake
cmake_minimum_required(VERSION 3.5)

add_definitions(
        -D_GNU_SOURCE
        -DTH_USE_CS0A
)

set(LIB_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/lib
        )

include_directories(
        ${LIB_INCLUDES}
)


set(LIB_SOURCES
        src/lib/lob.c
        src/lib/hashname.c
        src/lib/xht.c
        src/lib/js0n.c
        src/lib/base32.c
        src/lib/chacha.c
        src/lib/murmur.c
        src/lib/jwt.c
        src/lib/base64.c
        src/lib/aes128.c
        src/lib/sha256.c
        src/lib/uECC.c)
set(CIPHER_SOURCES src/e3x/cs0a/cs0a.c src/e3x/cs0a_disabled.c src/e3x/cs1a/cs1a.c src/e3x/cs3a_disabled.c src/e3x/cs2a_disabled.c)
set(E3X_SOURCES src/e3x/e3x.c src/e3x/self.c src/e3x/exchange.c src/e3x/cipher.c)
set(MESH_SOURCES src/mesh.c src/link.c src/chan.c src/pipe.c src/util/uri.c)
set(UTIL_SOURCES src/util/util.c src/util/chunks.c src/util/frames.c src/unix/util.c src/unix/util_sys.c)
file(GLOB TEST_SOURCES test/*.c)

add_library(telehash ${LIB_SOURCES} ${E3X_SOURCES} ${MESH_SOURCES} ${UTIL_SOURCES} ${CIPHER_SOURCES})
add_library(telehash_bl ${LIB_SOURCES} ${E3X_SOURCES} ${MESH_SOURCES} ${UTIL_SOURCES})
add_library(tests ${TEST_SOURCES})
add_executable(test_app util/app.c src/net/tcp4.c src/net/loopback.c src/net/udp4.c)
add_executable(loopback_test test/net_loopback.c src/net/loopback.c)
add_executable(chan_reliability_test test/chan_reliability.c)
target_link_libraries(test_app telehash)
target_link_libraries(loopback_test telehash)
target_link_libraries(chan_reliability_test telehash)
target_compile_definitions(telehash_bl PUBLIC NOLOG=true)
