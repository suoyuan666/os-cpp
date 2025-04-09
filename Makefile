U = build/utils
K = build/kernel

QEMU = qemu-system-riscv64

UPROGS=\
	$U/init \
	$U/sh \
	$U/cat  \
	$U/ls  \
	$U/test \

CPUS = 3

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 512M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

fs: 
	build/mkfs/mkfs fs.img --txt README.md --bin $(UPROGS)

qemu: fs.img
	$(QEMU) $(QEMUOPTS)

qemu-gdb: fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -s -S 

lldb: fs.img
	lldb --local-lldbinit
