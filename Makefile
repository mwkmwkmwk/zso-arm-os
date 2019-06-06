all: loader kernel

L_OBJ := loader.o lk.o
K_OBJ := kernel.o

lk.o: lk.s kernel
	clang --target=armv7-eabi -march=armv7 lk.s -c -ffreestanding

loader: $(L_OBJ)
	arm-none-eabi-ld $(L_OBJ) -o $@ -Ttext=0x8000

kernel: $(K_OBJ)
	arm-none-eabi-ld $(K_OBJ) -o $@ -Ttext=0x80001000

%.o: %.c
	#clang --target=armv7-eabi -march=armv7 $< -c -ffreestanding

	arm-none-eabi-gcc -march=armv7ve $< -c -ffreestanding
