set (KERNEL_NAME kernel)

set (
    KERNEL_SRC
    start.s
    main.cpp
)

set (CMAKE_ASM_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib")

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -ffreestanding -mcmodel=medany")

add_link_options("-T${CMAKE_SOURCE_DIR}/kernel/kernel.ld")

add_executable(${KERNEL_NAME} ${KERNEL_SRC})