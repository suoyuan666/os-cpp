set (ULIBCXX_NAME ulibcxx)

set (
    ULIBCXX_SRC
    ${PROJECT_SOURCE_DIR}/ulibc++/src/cstring.cpp
)

set(
    ULIBCXX_INCLUDE
    ${PROJECT_SOURCE_DIR}/ulibc++/include
)

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++ -ffreestanding -mcmodel=medany -fno-rtti -fno-exceptions")

add_library(${ULIBCXX_NAME} ${ULIBCXX_SRC})

target_include_directories(${ULIBCXX_NAME} PRIVATE ${ULIBCXX_INCLUDE})
