.PHONY = clean upload all linux/arch/x86/boot/bzImage linux

all: output.iso

iso/images/ramfs: $(shell find ramfs) ramfs/boot/System.map-3.5.4
	cd ramfs && find . | cpio -R 0:0 --quiet -H newc -o | gzip -9 > ../iso/images/ramfs

linux:
	@rm -rf linux linux-3.5.4.tar.bz2
	wget http://www.kernel.org/pub/linux/kernel/v3.0/linux-3.5.4.tar.bz2
	tar xjf linux-3.5.4.tar.bz2
	mv linux-3.5.4 linux
	cp source/kernel-config linux/.config
	cd linux && patch -p1 < ../source/linux-3.5.4-ptree.patch

linux/arch/x86/boot/bzImage: linux
	$(MAKE) -C linux

ramfs/boot/System.map-3.5.4: linux linux/arch/x86/boot/bzImage
	mkdir -p ramfs/boot
	cp linux/System.map ramfs/boot/System.map-3.5.4

iso/kernel/linux: linux/arch/x86/boot/bzImage
	cp linux/arch/x86/boot/bzImage iso/kernel/linux

output.iso: iso/images/ramfs iso/kernel/linux
	mkisofs -o output.iso -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table iso

clean:
	rm -rf output.iso iso/images/ramfs iso/kernel/linux linux ramfs/boot/System.map-3.5.4 linux-3.5.4.tar.bz2

upload: output.iso
	scp output.iso zx2c4.com:data.zx2c4.com/zx2c4-kernel-challenge.iso
