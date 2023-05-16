// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.5 -- Março de 2023

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

typedef enum status_enum 
{
	READY,
	RUNNING,
	SUSPENDED,
	TERMINATED
} status_e;

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
	struct task_t *prev, *next ;// ponteiros para usar em filas
	int id ;					// identificador da tarefa
	ucontext_t context ;		// contexto armazenado da tarefa
	status_e status ;			// pronta, rodando, suspensa, ...
	signed char p_sta ;			// valor de prioridade estatica da tarefa
	signed char p_din ;			// valor de prioridade dinamica da tarefa
	unsigned char quantum ;		// fatia de tempo para tarefa executar
	unsigned char user_task ;	// se tarefa de usuario entao 1, senao 0
  // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

// Variaveis globais ================================================

// estrutura para gestao de tarefas
typedef struct task_manager
{
	task_t tsk_main;		// armazena as informacoes da tarefa principal (main)
	task_t tsk_disp;		// armazena as informacoes da tarefa despachante (dispatcher)
	task_t *tsk_curr;		// ponteiro para a tarefa atual
	queue_t *q_tasks;		// ponteiro para a fila de tarefas
} manager_t;

// armazena o id que sera usado para uma nova tarefa
extern int id_new;

// armazena o ultimo id usado em uma tarefa
extern int id_last;

// estrutura que define um tratador de sinal (deve ser global ou static)
extern struct sigaction action ;

// estrutura de inicialização do timer
extern struct itimerval timer;

// bloqueador de interrupcao de ticks
extern int kernel_lock;

// Constantes =======================================================

#define ID_MAIN 0			/* numero identificador da tarefa principal */
#define ID_DISP 1			/* numero identificador da tarefa despachante */

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

#define DEFAULT_PRIORITY 0	/* prioridade de tarefas padrao */
#define MIN_PRIORITY -20	/* limite minimo para prioridade de tarefas */
#define MAX_PRIORITY  20	/* limite maximo para prioridade de tarefas */

#define DEFAULT_QUANTUM 20	/* quantum padrao das tarefas */
#define TICK_FREQ_IN_MS 1	/* frequencia do tick */

#define AGING_ALPHA -1		/* parametro para modificar prioridade de tarefas */

#endif
