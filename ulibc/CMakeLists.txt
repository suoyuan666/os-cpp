set (ULIBC_NAME ulibc)

set (
    ULIBC_SRC
    # ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__lockfile.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/internal/syscall_ret.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__stdio_write.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__towrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/fwrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/printf.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/stdout.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/vfprintf.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memchr.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memcpy.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memset.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strnlen.c
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

target_include_directories(${ULIBC_NAME} PRIVATE ${ULIBC_INCLUDE})
