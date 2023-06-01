// Lucas Correia de Araujo - GRR 20206150
// PingPongOS - PingPong Operating System
// 'ppos_data.h'
// -- Cabecalho com definicoes e prototipos
//
// Disciplina: Sistema Operacionais (CI1215)
// Professor: Carlos A. Maziero, DINF UFPR
//
// Maio de 2023
// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#ifdef _WIN32 
    #include <Windows.h>
#else
	#ifdef _WIN64
		#include <Windows.h>
	#else
		#include <unistd.h>
	#endif
#endif

// Enum para status das tarefas
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
	unsigned int proc_time ;	// tempo de processamento na cpu
	unsigned int exec_time ;	// tempo de vida da tarefa
	unsigned int activations ;	// numero de ativacoes na cpu
	struct task_t *waiting ;	// fila de tarefas que estao esperando pela tarefa 
	int exit_code ;				// codigo de encerramento da tarefa
	unsigned int wake_time ;	// tempo para tarefa acordar
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
	task_t   main;			// armazena as informacoes da tarefa principal (main)
	task_t   dispatcher;	// armazena as informacoes da tarefa despachante (dispatcher)
	task_t  *current;		// ponteiro para a tarefa atual
	queue_t *ready;			// ponteiro para a fila de tarefas prontas
	queue_t *sleeping;		// ponteiro para a fila de tarefas dormindo
	int id_new;				// armazena o id que sera usado para uma nova tarefa
} tsk_manager_t;

typedef struct timer_manager
{
	struct sigaction int_action;// estrutura que define um tratador de sinal
	struct itimerval int_timer; // estrutura de inicialização do timer
	unsigned char kernel_lock; // bloqueador de interrupcao de ticks
	unsigned int sys_timer;	// temporizador do sistema
} tmr_manager_t;

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
