set (PG_NAME sh)

set (
    UTILS_SRC
    ${PROJECT_SOURCE_DIR}/utils/test.cpp
)

set(
    UTILS_LIBCPP_INCLUDE
    ${PROJECT_SOURCE_DIR}/ulibc++/include
)

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -ffreestanding -mcmodel=medany")

add_executable(${PG_NAME} ${UTILS_SRC})

target_include_directories(${PG_NAME} PRIVATE ${UTILS_LIBCPP_INCLUDE})