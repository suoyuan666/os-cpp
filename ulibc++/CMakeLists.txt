set (ULIBCXX_NAME ulibcxx)

set (
    ULIBCXX_SRC
    ${PROJECT_SOURCE_DIR}/ulibc++/src/cstring.cpp
    ${PROJECT_SOURCE_DIR}/ulibc++/src/ostream.cpp
)

set(
    ULIBCXX_INCLUDE
    ${PROJECT_SOURCE_DIR}/ulibc++/include
    ${PROJECT_SOURCE_DIR}/kernel/include
)

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++ -ffreestanding -mcmodel=medany -fno-rtti -fno-exceptions -fno-stack-protector -fno-pie")

add_library(${ULIBCXX_NAME} ${ULIBCXX_SRC})

target_include_directories(${ULIBCXX_NAME} PRIVATE ${ULIBCXX_INCLUDE})
target_link_options(${ULIBCXX_NAME} PRIVATE "-Wl,-T${CMAKE_SOURCE_DIR}/utils/user.ld,-z,max-page-size=4096")
