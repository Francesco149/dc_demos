KOS_ROMDISK_DIR = romdisk_boot
all: rm-elf point_lights.elf

include $(KOS_BASE)/Makefile.rules

POINT_LIGHTSS = point_lights.o romdisk.o

clean:
	rm -f point_lights.elf $(POINT_LIGHTSS)

rm-elf:
	rm -f point_lights.elf
	rm -f romdisk.img

point_lights.elf: $(POINT_LIGHTSS)
	kos-cc \
		-Wall -Werror \
		-o point_lights.elf \
		$(POINT_LIGHTSS)

run: point_lights.elf
	$(KOS_LOADER) point_lights.elf

dist:
	rm -f $(POINT_LIGHTSS) romdisk.*
	$(KOS_STRIP) point_lights.elf
