#include <arch/timer.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <kernel/printf.h>

volatile int alarm_pending = 0;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	u64 sie = csr_read(CSR_SIE);
	sie |= CSR_SIE_STIE;
	csr_write(CSR_SIE, sie);
}

void timer_irq_disable()
{
	u64 sie = csr_read(CSR_SIE);
	sie &= ~CSR_SIE_STIE;
	csr_write(CSR_SIE, sie);
}

void timer_set_alarm(u64 secs)
{
	u64 now = timer_read();
	u64 ticks_in_future = now + (secs * TIMER_FREQ);
	csr_write(CSR_STIMECMP, ticks_in_future);
}

void timer_irq()
{
	csr_write(CSR_STIMECMP, -1ULL);
	
	if (alarm_pending) {
		printk(LOG_INFO, "\nalarm\n> ");
		alarm_pending = 0;
	}
}