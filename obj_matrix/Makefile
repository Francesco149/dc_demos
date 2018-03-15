KOS_ROMDISK_DIR = romdisk_boot
all: rm-elf obj_matrix.elf

include $(KOS_BASE)/Makefile.rules

OBJ_MATRIXS = obj_matrix.o romdisk.o

clean:
	rm -f obj_matrix.elf $(OBJ_MATRIXS)

rm-elf:
	rm -f obj_matrix.elf
	rm -f romdisk.img

obj_matrix.elf: $(OBJ_MATRIXS)
	kos-cc \
		-Wall -Werror \
		-o obj_matrix.elf \
		$(OBJ_MATRIXS)

run: obj_matrix.elf
	$(KOS_LOADER) obj_matrix.elf

dist:
	rm -f $(OBJ_MATRIXS) romdisk.*
	$(KOS_STRIP) obj_matrix.elf
