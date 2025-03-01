add_executable(mkfs mkfs.cpp)
target_include_directories(mkfs PRIVATE ${PROJECT_SOURCE_DIR}/kernel/include)
