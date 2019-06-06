#include <stdint.h>
asm (
	".global _start\n"
	"_start:\n"
	"ldr sp, =stack+0x1000\n"
	"b main\n"
);

asm (
	".global vectors\n"
	".balign 0x20\n"
	"vectors:\n"
	"nop\n"
	"b handle_und\n"
	"b handle_svc\n"
	"b handle_abort_i\n"
	"b handle_abort_d\n"
	"nop\n"
	"b handle_irq\n"
	"b handle_fiq\n"
);

asm (
	"handle_und: b handle_und\n"

	"handle_svc:\n"
	"push {r1, r2, r3, r12, lr}\n"
	"bl handle_svc_c\n"
	"pop {r1, r2, r3, r12, lr}\n"
	"movs pc, lr\n"

	"handle_abort_i: b handle_abort_i\n"
	"handle_abort_d: b handle_abort_d\n"

	"handle_irq:\n"
	"push {r0, r1, r2, r3, r12, lr}\n"
	"bl handle_irq_c\n"
	"pop {r0, r1, r2, r3, r12, lr}\n"
	"sub lr, lr, #4\n"
	"movs pc, lr\n"

	"handle_fiq: b handle_fiq\n"
);

char stack[0x1000];
char stack_svc[0x1000];
char stack_irq[0x1000];
extern char vectors[];


static const volatile unsigned int *const uart_sr = (void *)0xe000002c;
static volatile unsigned int *const uart_isr = (void *)0xe0000014;
static volatile unsigned int *const uart_fifo = (void *)0xe0000030;
static volatile unsigned int *const ICCICR = (void *)0xf8f00100;
static volatile unsigned int *const ICCPMR = (void *)0xf8f00104;
static volatile unsigned int *const ICCIAR = (void *)0xf8f0010c;
static volatile unsigned int *const ICCEOIR = (void *)0xf8f00110;
static volatile unsigned int *const ICCRPR = (void *)0xf8f00114;
static volatile unsigned int *const ICCHPIR = (void *)0xf8f00118;
static volatile unsigned int *const ICDDCR = (void *)0xf8f01000;
static volatile unsigned int *const ICDISR = (void *)0xf8f01080;
static volatile unsigned int *const ICDISER = (void *)0xf8f01100;
static volatile unsigned int *const ICDISPR = (void *)0xf8f01200;
static volatile unsigned int *const ICDABR = (void *)0xf8f01300;
static volatile unsigned int *const ICDIPR = (void *)0xf8f01400;
static volatile unsigned int *const ICDIPTR = (void *)0xf8f01800;
static volatile unsigned int *const SPI_STATUS = (void *)0xf8f01d04;
static volatile unsigned int *const UART_IER = (void *)0xE0000008;
static volatile unsigned int *const UART_RTRIG = (void *)0xE0000020;

void putc(char c) {
	while (*uart_sr & 1 << 4);
	*uart_fifo = c;
}

void print_hex(unsigned x) {
	for (int i = 0; i < 8; i++) {
		int hexit = x >> ((7 - i) * 4) & 0xf;
		putc(hexit["0123456789abcdef"]);
	}
}

void printf(const char *fmt) {
	char c;
	while ((c = *fmt++))
		putc(c);
}

void handle_uart() {
	printf("before ");
	print_hex(*uart_sr);
	print_hex(*uart_isr);
	*uart_isr = 1;
	while (!(*uart_sr & 2)) {
		printf(" after ");
		print_hex(*uart_sr);
		print_hex(*uart_isr);
		unsigned char c = *uart_fifo;
		printf(" after ");
		print_hex(*uart_sr);
		print_hex(*uart_isr);
		printf("\r\n");
		int lo = c & 0xf;
		int hi = c >> 4;
		putc(hi["0123456789abcdef"]);
		putc(lo["0123456789abcdef"]);
		printf("\r\n");
	}
}

void handle_irq_c(void) {
	unsigned irq = *ICCIAR;
	unsigned res;
	printf("IRQ ");
	print_hex(irq);
	printf("\r\n");
	asm volatile ("mrs %0, cpsr" :"=r"(res):);
	print_hex(res);
	printf("\r\n");
	asm volatile ("mrs %0, spsr" :"=r"(res):);
	print_hex(res);
	printf("\r\n");
	asm volatile ("mov %0, sp" :"=r"(res):);
	print_hex(res);
	printf("\r\n");
	if (irq == 0x3ff)
		return;
	if (irq == 59) {
		handle_uart();
	}
	*ICCEOIR = irq;
}

int handle_svc_c(unsigned a, unsigned b, unsigned c, unsigned d) {
	unsigned res;
	asm volatile ("mrs %0, cpsr" :"=r"(res):);
	print_hex(res);
	printf("\r\n");
	asm volatile ("mrs %0, spsr" :"=r"(res):);
	print_hex(res);
	printf("\r\n");
	printf("Syscall\r\n");
	return 1337;
}

_Noreturn void main(void) {
	printf("Hello, world!\r\n");
	printf("Hello, world!\r\n");
	printf("Hello, world!\r\n");
	printf("Hello, world!\r\n");
	volatile int delay = 10000000;
	while(delay--);

	register int res asm("r0");
	asm volatile ("mcr p15, #0, %0, c12, c0, #0" :: "r"(vectors));
	asm volatile ("mcr p15, #0, %0, c12, c0, #1" :: "r"(vectors));
	printf("VBAR!\r\n");
	delay = 10000000;
	while(delay--);

	asm volatile (
		"msr cpsr_xc, #0xd3\n"
		"mov sp, %0\n"
		"msr cpsr_xc, #0xd2\n"
		"mov sp, %1\n"
		"msr cpsr_xc, #0xdf\n"
		::
		"r"(stack_svc + 0x1000),
		"r"(stack_irq + 0x1000)
		: "lr"
	);
	printf("CPSR!\r\n");
	delay = 10000000;
	while(delay--);

	*ICCICR = 7;
	*ICCPMR = 0xff;
	*ICDDCR = 3;
	*UART_RTRIG = 1;
	*UART_IER = 1;
	ICDISR[1] = 1 << (59 - 32);
	ICDISER[1] = 1 << (59 - 32);
	ICDIPR[59 / 4] = 0 << ((59 % 4) * 8);
	ICDIPTR[59 / 4] = 1 << ((59 % 4) * 8);

	asm volatile ("msr cpsr_xc, %0" :: "r"(0x1f));

	printf("RUN!\r\n");
	delay = 10000000;
	while(delay--);

	while(1) asm volatile ("wfi");

#if 0
#endif
}
