set (KERNEL_NAME kernel)

set (
    KERNEL_SRC
    start.S
    main.cpp
    uart.cpp
    console.cpp
    kernelvec.S
    trampoline.S
    swtch.S
    vm.cpp
    trap.cpp
    syscall.cpp
    plic.cpp
    bio.cpp
    file.cpp
    pipe.cpp
    fs.cpp
    log.cpp
    proc.cpp
    virtio_disk.cpp
    log.cpp
    lock.cpp
    loader.cpp
    sysfile.cpp
)

set (
  KERNEL_INCLUDE
  ${PROJECT_SOURCE_DIR}/kernel
  ${PROJECT_SOURCE_DIR}/kernel/include
  ${PROJECT_SOURCE_DIR}/kernel/micro_libcxx/include
)

set (CMAKE_ASM_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64d -nostdlib -nostdlib++")

set (CMAKE_CXX_FLAGS "-target riscv64-unknown-elf -march=rv64g -mabi=lp64d -nostdlib -nostdlib++ -ffreestanding -mcmodel=medany -fno-rtti -fno-exceptions")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-common")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcpy -Wno-main")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset")

add_subdirectory(micro_libcxx)

add_executable(${KERNEL_NAME} ${KERNEL_SRC})

target_link_options(${KERNEL_NAME} PRIVATE "-Wl,-T${CMAKE_SOURCE_DIR}/kernel/kernel.ld")

target_include_directories(${KERNEL_NAME} PRIVATE ${KERNEL_INCLUDE})
target_link_libraries(${KERNEL_NAME} PRIVATE micro_libcxx)
