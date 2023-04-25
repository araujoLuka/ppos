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
  struct task_t *prev, *next ;		// ponteiros para usar em filas
  int id ;				// identificador da tarefa
  ucontext_t context ;			// contexto armazenado da tarefa
  status_e status ;			// pronta, rodando, suspensa, ...
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

// armazena as informacoes da tarefa principal (main)
extern task_t tsk_main;

// armazena as informacoes da tarefa despachante (dispatcher)
extern task_t tsk_disp;

// ponteiro para a tarefa atual
extern task_t *tsk_curr;

// armazena o id que sera usado para uma nova tarefa
extern int id_new;

// armazena o ultimo id usado em uma tarefa
extern int id_last;

// ponteiro para a fila de tarefas
extern queue_t *q_tasks;

#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

#endif

