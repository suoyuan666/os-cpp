set (KLIBCXX_NAME micro_libcxx)

set (
    KLIBCXX_SRC
    ${PROJECT_SOURCE_DIR}/kernel/micro_libcxx/src/cstring.cpp
)

set(
    KLIBCXX_INCLUDE
    ${PROJECT_SOURCE_DIR}/kernel/micro_libcxx/include
    ${PROJECT_SOURCE_DIR}/kernel/include
)

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++ -ffreestanding -mcmodel=medany -fno-rtti -fno-exceptions -fno-stack-protector -fno-pie")

add_library(${KLIBCXX_NAME} ${KLIBCXX_SRC})

target_include_directories(${KLIBCXX_NAME} PRIVATE ${KLIBCXX_INCLUDE})
