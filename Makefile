qemu:
	qemu-system-riscv64 -machine virt -bios none -nographic -m 512M -kernel build/kernel/kernel \

qemu-gdb:
	qemu-system-riscv64 -machine virt -bios none -nographic -m 512M -kernel build/kernel/kernel \
											-s -S