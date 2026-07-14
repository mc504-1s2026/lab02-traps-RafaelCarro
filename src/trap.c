#include <kernel/trap.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <kernel/serial.h>

/* Declarado em src/trap_entry.S */
extern void trap_entry();

/* 
 * Função para configurar o vetor de traps da CPU.
 * O endereço do tratador em assembly (trap_entry) é colocado no registrador stvec.
 */
void trap_setup()
{
	// Escreve o endereço da nossa rotina de assembly no registrador STVEC
	csr_write(CSR_STVEC, (u64)trap_entry);
}

/*
 * Tratador de interrupções físicas (IRQ)
 */
void handle_irq()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 irq_code = cause & ~(1UL << 63);

	switch (irq_code) {
		case 5: // Supervisor Timer Interrupt (STIP)
			timer_irq();
			break;
		case 9: // Supervisor External Interrupt (SEIP)
			serial_irq(); // <-- AGORA conectado de forma correta ao driver serial!
			break;
		default:
			panic("Erro: Interrupcao (IRQ) nao implementada: %d\n", irq_code);
	}
}

/*
 * Tratador de exceções síncronas (instruções inválidas, falhas de paginação, etc.)
 */
void handle_exception()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 bad_addr = csr_read(CSR_STVAL);
	u64 epc = csr_read(CSR_SEPC);

	// Pretty-printing de erros graves para facilitar a depuração no terminal
	panic("EXCECAO SINCRONA DETECTADA!\n"
	      "  scause: 0x%lx (Code: %ld)\n"
	      "  sepc  (Endereco de quebra): 0x%lx\n"
	      "  stval (Endereco de memoria problemático): 0x%lx\n",
	      cause, cause & 0xfff, epc, bad_addr);
}

/*
 * Ponto de entrada chamado diretamente pelo stub assembly trap_entry
 */
void handle_trap()
{
	u64 cause = csr_read(CSR_SCAUSE);

	// Se o bit mais significativo (63) estiver setado, trata-se de uma interrupção (IRQ)
	if (cause & (1UL << 63)) {
		handle_irq();
	} else {
		handle_exception();
	}
}

/* 
 * Habilita as interrupções gerais (de forma síncrona/segura) no sstatus (bit SIE)
 */
void hart_irq_enable()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	sstatus |= CSR_SSTATUS_SIE;
	csr_write(CSR_SSTATUS, sstatus);
}

/*
 * Desabilita as interrupções globais e retorna as flags de estado anteriores
 */
u64 hart_irq_save()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	u64 flags = sstatus & CSR_SSTATUS_SIE;
	
	sstatus &= ~CSR_SSTATUS_SIE;
	csr_write(CSR_SSTATUS, sstatus);
	
	return flags;
}

/*
 * Restaura o estado das interrupções globais de acordo com as flags fornecidas
 */
void hart_irq_restore(u64 flags)
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	
	if (flags & CSR_SSTATUS_SIE) {
		sstatus |= CSR_SSTATUS_SIE;
	} else {
		sstatus &= ~CSR_SSTATUS_SIE;
	}
	
	csr_write(CSR_SSTATUS, sstatus);
}

/*
 * Desabilita totalmente as interrupções globais
 */
void hart_irq_disable()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	sstatus &= ~CSR_SSTATUS_SIE;
	csr_write(CSR_SSTATUS, sstatus);
}