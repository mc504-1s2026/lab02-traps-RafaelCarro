#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>

#define CMD_BUF_SIZE 256
#define TIMER_FREQ 10000000UL

extern int _hartid[];
extern volatile int alarm_pending;

/* Função auxiliar para converter string em inteiro (atoi simplificado) */
static u64 k_atoi(const char *str)
{
	u64 res = 0;
	while (*str >= '0' && *str <= '9') {
		res = res * 10 + (*str - '0');
		str++;
	}
	return res;
}

/* Processa e executa os comandos digitados no Shell */
static void execute_command(char *cmd_line)
{
	// Remove espaços em branco iniciais
	while (*cmd_line == ' ' || *cmd_line == '\t') {
		cmd_line++;
	}

	if (*cmd_line == '\0') {
		return;
	}

	char *cmd = cmd_line;
	char *arg = NULL;
	
	char *p = cmd_line;
	while (*p != '\0') {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			arg = p + 1;
			while (*arg == ' ' || *arg == '\t') {
				arg++;
			}
			break;
		}
		p++;
	}

	if (strcmp(cmd, "uptime") == 0) {
		u64 ticks = timer_read();
		u64 uptime_s = ticks / TIMER_FREQ;
		printk("%lds\n", uptime_s);
	}
	else if (strcmp(cmd, "echo") == 0) {
		if (arg && *arg != '\0') {
			printk("%s\n", arg);
		} else {
			printk("\n");
		}
	}
	else if (strcmp(cmd, "alarm") == 0) {
		if (arg && *arg != '\0') {
			u64 secs = k_atoi(arg);
			if (secs > 0) {
				alarm_pending = 1;
				timer_set_alarm(secs);
				// Imprime uma confirmação opcional ou simplesmente prossegue
			} else {
				printk("alarm: tempo invalido\n");
			}
		} else {
			printk("Uso: alarm [tempo_em_segundos]\n");
		}
	}
	else {
		printk("Comando nao encontrado: %s\n", cmd);
	}
}

void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");[cite: 5]
	info("booting on hart %d\n", _hartid[0]);[cite: 5]
	info("setting up virtual memory...\n");[cite: 5]
	vm_init();[cite: 5]

	info("enabling traps...\n");[cite: 5]
	trap_setup();[cite: 5]
	info("enabling timer...\n");[cite: 5]
	timer_irq_enable();[cite: 5]
	info("enabling serial...\n");[cite: 5]
	serial_init();[cite: 5]
	serial_irq_enable();[cite: 5]

	hart_irq_enable();

	printk("\n--- RISC-V OS Kernel Shell ---\n");
	printk("> ");

	char cmd_buf[CMD_BUF_SIZE];
	size_t cmd_len = 0;
	memset(cmd_buf, 0, CMD_BUF_SIZE);

	while (1) {
		char input_char;
		size_t read_bytes = serial_read(&input_char);

		if (read_bytes > 0) {
			if (input_char == '\r' || input_char == '\n') {
				serial_putc('\n'); // Salta linha no terminal do usuário
				cmd_buf[cmd_len] = '\0';
				
				execute_command(cmd_buf);
				
				cmd_len = 0;
				memset(cmd_buf, 0, CMD_BUF_SIZE);
				printk("> ");
			}
			else if (input_char == '\b' || input_char == 127) {
				if (cmd_len > 0) {
					cmd_len--;
					cmd_buf[cmd_len] = '\0';
					serial_puts("\b \b");
				}
			}
			else {
				if (cmd_len < CMD_BUF_SIZE - 1) {
					cmd_buf[cmd_len++] = input_char;
					serial_putc(input_char);
				}
			}
		}

		asm volatile("wfi");
	}
}