KOS_ROMDISK_DIR = romdisk_boot
all: rm-elf quaternion_interpolation.elf

include $(KOS_BASE)/Makefile.rules

QUATERNION_INTERPOLATIONS = quaternion_interpolation.o romdisk.o

clean:
	rm -f quaternion_interpolation.elf $(QUATERNION_INTERPOLATIONS)

rm-elf:
	rm -f quaternion_interpolation.elf
	rm -f romdisk.img

quaternion_interpolation.elf: $(QUATERNION_INTERPOLATIONS)
	kos-cc \
		-Wall -Werror \
		-o quaternion_interpolation.elf \
		$(QUATERNION_INTERPOLATIONS) \
		-lm

run: quaternion_interpolation.elf
	$(KOS_LOADER) quaternion_interpolation.elf

dist:
	rm -f $(QUATERNION_INTERPOLATIONS) romdisk.*
	$(KOS_STRIP) quaternion_interpolation.elf
