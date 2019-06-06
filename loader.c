#include <stdint.h>
#include <elf.h>
#include <stddef.h>
asm (
	".global _start\n"
	"_start:\n"
	"msr cpsr_xc, #0xdf\n"
	"ldr sp, =stack+0x1000\n"
	"b main\n"
);

extern char kernel[];

char stack[0x1000];

__attribute__((aligned(1<<14))) uint32_t pt_l1[1 << 12] = {0};
__attribute__((aligned(1<<10))) uint32_t pt_l2[16][1 << 8] = {0};
int pt_l2_idx = 0;
extern char _end[];
uint32_t alloc_end = (uint32_t)&_end;

static const volatile unsigned int *const uart_sr = (void *)0xe000002c;
static volatile unsigned int *const uart_isr = (void *)0xe0000014;
static volatile unsigned int *const uart_fifo = (void *)0xe0000030;

void *memcpy(void *d, void *s, size_t sz) {
	char *cd = d;
	char *cs = s;
	while (sz--)
		*cd++ = *cs++;
	return d;
}

void *memset(void *d, int s, size_t sz) {
	char *cd = d;
	while (sz--)
		*cd++ = s;
	return d;
}

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

_Noreturn void main(void) {
	uint32_t sctlr;
	int i;
	for (i = 0 ; i < (1 << 9); i++) {
		pt_l1[i] = i << 20 | 0xc1e;
	}
	int uart_idx = 0xe0000000 >> 20;
	pt_l1[0xe0000000u >> 20] = uart_idx << 20 | 0xc12;
	int gic_idx = 0xf8f00000 >> 20;
	pt_l1[0xf8f00000u >> 20] = gic_idx << 20 | 0xc12;

	asm volatile ("mrc p15, #0, %0, c1, c0, #0" : "=r"(sctlr):);
	sctlr &= ~1;
	asm volatile ("mcr p15, #0, %0, c1, c0, #0" :: "r"(sctlr));

	uint32_t ttbr = (uint32_t)pt_l1 | 0x59;
	asm volatile ("mcr p15, #0, %0, c2, c0, #0" :: "r"(ttbr));
	asm volatile ("mcr p15, #0, %0, c2, c0, #1" :: "r"(ttbr));
	asm volatile ("mcr p15, #0, %0, c2, c0, #2" :: "r"(0));
	asm volatile ("mrc p15, #0, %0, c1, c0, #0" : "=r"(sctlr):);
	sctlr |= 1;
	asm volatile ("mcr p15, #0, %0, c1, c0, #0" :: "r"(sctlr));
	asm volatile ("mcr p15, #0, %0, c8, c7, #0" :: "r"(0));

	printf("Hello, loader!\r\n");

	unsigned volatile delay = 10000000;
	while(delay--);

	alloc_end--;
	alloc_end |= 0xfff;
	alloc_end++;

	Elf32_Ehdr *ehdr = (void*)kernel;
	Elf32_Phdr *phdr = (void*)&kernel[ehdr->e_phoff];
	for (int i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;
		uint32_t start = phdr[i].p_vaddr;
		uint32_t end = start + phdr[i].p_memsz;
		uint32_t astart = start & ~0xfff;
		for (uint32_t pos = astart; pos < end; pos += 0x1000) {
			printf("PAGE ");
			int l1i = pos >> 20;
			int l2i = pos >> 12 & 0xff;
			print_hex(pos);
			printf(" ");
			print_hex(l1i);
			printf(" ");
			print_hex(l2i);
			printf(" ");
			uint32_t *pl2;
			if (!pt_l1[l1i]) {
				pl2 = pt_l2[pt_l2_idx++];
				print_hex(pl2);
				printf(" ");
				pt_l1[l1i] = (uint32_t)pl2 | 1;
			} else {
				pl2 = (void *)(pt_l1[l1i] & ~0xff);
				print_hex(pl2);
				printf("*");
			}
			uint32_t pa = alloc_end;
			alloc_end += 0x1000;
			pl2[l2i] = pa | 0x13;
			print_hex(pl2[l2i]);
			printf("\r\n");
		}
		asm volatile ("mcr p15, #0, %0, c8, c7, #0" :: "r"(0));
		memset((void *)start, 0, phdr[i].p_memsz);
		memcpy((void *)start, &kernel[phdr[i].p_offset], phdr[i].p_filesz);
	}
	asm volatile ("mcr p15, #0, %0, c8, c7, #0" :: "r"(0));
	asm volatile ("mcr p15, #0, %0, c7, c5, #0" :: "r"(0));
	printf("Still alive.\r\n");
	print_hex(ehdr->e_entry);
	print_hex(*(uint32_t*)ehdr->e_entry);
	print_hex(*(uint32_t*)0x80014000);
	print_hex(*(uint32_t*)0x80001474);
	printf("Entering.\r\n");
	//asm volatile ("sync");
	((void(*)(void))ehdr->e_entry)();
	__builtin_unreachable();
}
