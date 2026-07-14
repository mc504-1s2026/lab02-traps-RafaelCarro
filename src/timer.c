#include <arch/timer.h>
#include <kernel/panic.h>
#include <arch/csr.h>

#define TIMER_FREQ 10000000UL

volatile int alarm_pending = 0;

/*
 * Lê o número de ticks acumulados desde o boot da CPU
 */
u64 timer_read()
{
	return csr_read(CSR_TIME);
}

/*
 * Habilita as interrupções de timer no registrador sie (Supervisor Interrupt Enable)
 */
void timer_irq_enable()
{
	u64 sie = csr_read(CSR_SIE);
	sie |= CSR_SIE_STIE; // Define o bit Supervisor Timer Interrupt Enable
	csr_write(CSR_SIE, sie);
}

/*
 * Desabilita as interrupções de timer no registrador sie
 */
void timer_irq_disable()
{
	u64 sie = csr_read(CSR_SIE);
	sie &= ~CSR_SIE_STIE; // Limpa o bit Supervisor Timer Interrupt Enable
	csr_write(CSR_SIE, sie);
}

/*
 * Configura um alarme absoluto para "secs" segundos a partir de AGORA
 */
void timer_set_alarm(u64 secs)
{
	u64 now = timer_read();
	u64 ticks_in_future = now + (secs * TIMER_FREQ);
	
	csr_write(CSR_STIMECMP, ticks_in_future);
}

/*
 * Tratador que será chamado quando a interrupção de timer disparar.
 */
void timer_irq()
{
	// Desativa o alarme atual (define stimecmp para infinito para não disparar de novo)
	csr_write(CSR_STIMECMP, -1ULL);
	
	if (alarm_pending) {
		printk("\nalarm\n> "); // Exibe o alarme assincronamente e redesenha o prompt do shell
		alarm_pending = 0;
	}
}