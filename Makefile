OBJ := h4x.o

h4x: $(OBJ)
	ld.lld $(OBJ) -o h4x -Ttext=0x8000

%.o: %.c
	clang --target=armv7-eabi $< -c -ffreestanding
