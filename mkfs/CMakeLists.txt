add_executable(mkfs mkfs.cpp)
target_include_directories(mkfs PRIVATE ${PROJECT_SOURCE_DIR}/kernel/include)
set(CMAKE_CXX_FLAGS "-pipe -O3 -march=native -flto=thin -fsanitize=cfi -fvisibility=hidden -fstack-protector-strong -fstack-clash-protection -fcf-protection=full -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3")
target_link_options(mkfs PRIVATE "-Wl,-O3,-z,now,-z,noexecstack,--as-needed,--lto-O3,--icf=safe,--gc-sections")
