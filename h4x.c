asm (
	".global _start\n"
	"_start:\n"
	"ldr sp, =stack+0x1000\n"
	"b main\n"
);

char stack[0x1000];

static const volatile unsigned int *const uart_sr = (void *)0xe000002c;
static volatile unsigned int *const uart_fifo = (void *)0xe0000030;

char getc(void) {
	while (*uart_sr & 1 << 1);
	return *uart_fifo;
}

void putc(char c) {
	while (*uart_sr & 1 << 4);
	*uart_fifo = c;
}

void printf(const char *fmt) {
	char c;
	while ((c = *fmt++)) {
		putc(c);
	}
}

_Noreturn void main(void) {
	printf("Hello, world!\n");
	while(1) {
		unsigned char c = getc();
		int lo = c & 0xf;
		int hi = c >> 4;
		putc(hi["0123456789abcdef"]);
		putc(lo["0123456789abcdef"]);
		printf("\r\n");
	}
}
