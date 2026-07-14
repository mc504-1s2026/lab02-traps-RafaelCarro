#include <kernel/serial.h>
#include <kernel/panic.h>
#include <kernel/string.h>
#include <arch/spinlock.h>
#include <arch/plic.h>
#include <arch/csr.h>

#define UART_BASE 0x10000000UL
#define UART_IRQ  10

#define RBR (UART_BASE + 0)
#define THR (UART_BASE + 0)
#define IER (UART_BASE + 1)
#define FCR (UART_BASE + 2)
#define LCR (UART_BASE + 3)
#define LSR (UART_BASE + 5)

#define SERIAL_BUF_SIZE 512

struct serial_device {
	char buf[SERIAL_BUF_SIZE];
	volatile size_t head;
	volatile size_t tail;
	struct spinlock lock;
};

static struct serial_device s_dev = {
	.head = 0,
	.tail = 0,
	.lock = {0}
};

static inline void uart_write_reg(u64 reg, u8 val)
{
	*(volatile u8 *)reg = val;
}

static inline u8 uart_read_reg(u64 reg)
{
	return *(volatile u8 *)reg;
}

void serial_init()
{
	spin_init(&s_dev.lock);

	uart_write_reg(IER, 0x00); // Desabilita interrupções temporariamente
	uart_write_reg(LCR, 0x03); // 8n1
	uart_write_reg(FCR, 0x07); // Habilita e limpa FIFOs
	uart_write_reg(IER, 0x01); // Habilita interrupção de recebimento (Rx)
}

void serial_irq_enable()
{
	plic_irq_set_priority(UART_IRQ, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, UART_IRQ);

	u64 sie = csr_read(CSR_SIE);
	sie |= CSR_SIE_SEIE;
	csr_write(CSR_SIE, sie);
}

void serial_irq_disable()
{
	u64 sie = csr_read(CSR_SIE);
	sie &= ~CSR_SIE_SEIE;
	csr_write(CSR_SIE, sie);
}

/* 
 * Tratador de interrupção: roda com interrupções globais já desativadas pela CPU.
 * Usamos apenas o spinlock básico para proteger a escrita concorrente.
 */
void serial_irq()
{
	u32 irq = plic_hart_claim_irq(0);

	if (irq == UART_IRQ) {
		spin_lock(&s_dev.lock);

		while (uart_read_reg(LSR) & 0x01) {
			u8 c = uart_read_reg(RBR);
			size_t next_head = (s_dev.head + 1) % SERIAL_BUF_SIZE;

			if (next_head != s_dev.tail) {
				s_dev.buf[s_dev.head] = (char)c;
				s_dev.head = next_head;
			}
		}

		spin_unlock(&s_dev.lock);
	}

	if (irq != 0) {
		plic_hart_complete_irq(0, irq);
	}
}

/*
 * Leitura pelo loop principal (contexto de aplicação):
 * Desativa interrupções temporariamente de forma segura para evitar que a 
 * própria IRQ interrompa a leitura no meio do caminho.
 */
size_t serial_read(char *buf, size_t max_len)
{
	size_t count = 0;
	u64 flags;

	flags = spin_lock_irqsave(&s_dev.lock);

	while (s_dev.tail != s_dev.head && count < max_len) {
		buf[count++] = s_dev.buf[s_dev.tail];
		s_dev.tail = (s_dev.tail + 1) % SERIAL_BUF_SIZE;
	}

	spin_unlock_irqrestore(&s_dev.lock, flags);

	return count;
}

void serial_putc(char c)
{
	while ((uart_read_reg(LSR) & 0x20) == 0);
	uart_write_reg(THR, (u8)c);
}

void serial_puts(char *str)
{
	if (!str) return;

	while (*str != '\0') {
		if (*str == '\n') {
			serial_putc('\r');
		}
		serial_putc(*str);
		str++;
	}
}