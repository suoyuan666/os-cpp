set (KERNEL_NAME kernel)

set (
    KERNEL_SRC
    start.s
    main.cpp
    uart.cpp
)

set (
  KERNEL_INCLUDE
  ${PROJECT_SOURCE_DIR}/kernel
  ${PROJECT_SOURCE_DIR}/kernel/include
  ${PROJECT_SOURCE_DIR}/ulibc++/include
)

set (CMAKE_ASM_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++")

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64 -nostdlib -nostdlib++ -ffreestanding -mcmodel=medany -fno-rtti -fno-exceptions")

add_link_options("-T${CMAKE_SOURCE_DIR}/kernel/kernel.ld")

add_executable(${KERNEL_NAME} ${KERNEL_SRC})

target_include_directories(${KERNEL_NAME} PRIVATE ${KERNEL_INCLUDE})
target_link_libraries(${KERNEL_NAME} ulibc++)
