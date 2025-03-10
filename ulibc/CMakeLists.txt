set (ULIBC_NAME ulibc)

set (
    ULIBC_SRC
    # syscall
    ${PROJECT_SOURCE_DIR}/ulibc/src/internal/syscall_ret.c
    # fnctl
    ${PROJECT_SOURCE_DIR}/ulibc/src/fnctl/open.c
    # process
    ${PROJECT_SOURCE_DIR}/ulibc/src/process/wait.c
    # stat
    ${PROJECT_SOURCE_DIR}/ulibc/src/stat/fstat.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stat/mknod.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stat/stat.c
    # stdio
    # ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__lockfile.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__stdio_read.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__stdio_write.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__toread.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__towrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/__uflow.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/fgets.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/fwrite.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/gets.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/printf.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/stdin.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/stdout.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdio/vfprintf.c
    # string
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdlib/_Exit.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/stdlib/exit.c
    # string
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memchr.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memcpy.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memmove.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/memset.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strcpy.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strcpy.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strlen.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/string/strnlen.c
    # unistd
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/close.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/dup.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/execve.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/fork.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/read.c
    ${PROJECT_SOURCE_DIR}/ulibc/src/unistd/write.c
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
