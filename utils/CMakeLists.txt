set(
    UTILS_LIBCPP_INCLUDE
    ${PROJECT_SOURCE_DIR}/ulibc++/include
    ${PROJECT_SOURCE_DIR}/ulibc/include
    ${PROJECT_SOURCE_DIR}/ulibc/obj/include
    ${PROJECT_SOURCE_DIR}/kernel/include
)

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++ -mcmodel=medany")
set (CMAKE_C_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -mcmodel=medany -fno-stack-protector -fno-pie")
set (CMAKE_ASM_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -nostdlib -Wno-main -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc -fno-builtin-free -fno-builtin-memcpy -Wno-main -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf")

add_executable(init ${PROJECT_SOURCE_DIR}/utils/init.c)
add_executable(cat ${PROJECT_SOURCE_DIR}/utils/cat.c)
add_executable(sh ${PROJECT_SOURCE_DIR}/utils/sh.c)
add_executable(initcode ${PROJECT_SOURCE_DIR}/utils/initcode.S)

include_directories(${UTILS_LIBCPP_INCLUDE})

target_link_libraries(init PRIVATE crt1 ulibc)
target_link_options(init PRIVATE "-Wl,-T${CMAKE_SOURCE_DIR}/utils/user.ld,-z,max-page-size=4096")

target_link_libraries(cat PRIVATE crt1 ulibc)
target_link_options(cat PRIVATE "-Wl,-T${CMAKE_SOURCE_DIR}/utils/user.ld,-z,max-page-size=4096")

target_link_libraries(sh PRIVATE crt1 ulibc)
target_link_options(sh PRIVATE "-Wl,-T${CMAKE_SOURCE_DIR}/utils/user.ld,-z,max-page-size=4096")

target_link_options(initcode PRIVATE "-Wl,-N,-e,start,-Ttext,0")
