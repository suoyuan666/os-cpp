set (ULIBC_NAME ulibc)

set (
    ULIBC_SRC
    # syscall
    ${PROJECT_SOURCE_DIR}/ulibc/src/internal/syscall_ret.c
    # stdio
    # ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__lockfile.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__stdio_write.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__towrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/fwrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/printf.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/stdout.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/vfprintf.c
    # string
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memchr.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memcpy.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memset.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strnlen.c
    # fnctl
    ${PROJECT_SOURCE_DIR}/ulibc/src/fnctl/open.c
    # unistd
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/dup.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/write.c
    # stat
    ${PROJECT_SOURCE_DIR}/ulibc/src/stat/mknod.c
)

set (
    CRT_SRC
    ${PROJECT_SOURCE_DIR}/ulibc/crt/crt1.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/internal/syscall_ret.c

)

set(
    ULIBC_INCLUDE
    ${PROJECT_SOURCE_DIR}/ulibc/include
    ${PROJECT_SOURCE_DIR}/ulibc/src/include
    ${PROJECT_SOURCE_DIR}/ulibc/obj/include
    ${PROJECT_SOURCE_DIR}/ulibc/arch/riscv64
)

set (CMAKE_C_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -ffreestanding -mcmodel=medany")

add_library(${ULIBC_NAME} ${ULIBC_SRC})
add_library(crt1 ${CRT_SRC})

target_include_directories(${ULIBC_NAME} PRIVATE ${ULIBC_INCLUDE})
target_include_directories(crt1 PRIVATE ${ULIBC_INCLUDE})
