// PingPongOS - PingPong Operating System
// 'ppos_core.c'
// -- Codigo nucleo do sistema operacional
//
// Autor: Lucas Correia de Araujo
// Disciplina: Sistema Operacionais (CI1215)
// Professor: Carlos A. Maziero, DINF UFPR
//
// Abril de 2023
// Funcoes e operacoes internas do sistema

#include "ppos.h"

task_t tsk_main, *tsk_curr;
int id_new, id_last;

//------------------------------------------------------------------------------
// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void ppos_init ()
{
	#ifdef DEBUG
	printf("PPOS: ppos_init > system initialized\n");
	#endif /* !DEBUG */

	ucontext_t a;
	getcontext(&a);

	tsk_main.id = 0;
	tsk_main.context = a;
	tsk_main.next = NULL;
	tsk_main.prev = NULL;
	tsk_main.status = RUNNING;

	tsk_curr = &tsk_main;

	id_last = tsk_main.id;
	id_new = tsk_main.id + 1;

	setvbuf(stdout, 0, _IONBF, 0);
}

// Gerência de tarefas =========================================================

//------------------------------------------------------------------------------
// Inicializa uma nova tarefa. Retorna um ID> 0 ou erro.

int task_init (task_t *task, void (*start_func)(void *), void *arg)
{
	char *stack;

	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot initialize a null task\n");
		return -1;
	}

	task->id = id_new++;
	#ifdef DEBUG
	printf("PPOS: task_init > task %d created by task %d (func %p)\n", task->id, tsk_curr->id, start_func);
	#endif /* !DEBUG */

	// salva as informacoes do contexto atual para
	// ser usado como base do novo contexto
	getcontext(&task->context);

	// aloca espaco para a stack do contexto a ser criado
	stack = malloc(STACKSIZE);
	if (stack)
	{
		task->context.uc_stack.ss_sp = stack;
		task->context.uc_stack.ss_size = STACKSIZE;
		task->context.uc_stack.ss_flags = 0;
		task->context.uc_link = 0;
	} 
	else
	{
		fprintf(stderr, "ERROR: failure to create stack for task %d\n", task->id);
		return -2;
	}

	// cria o contexto da tarefa
	makecontext(&task->context, (void*)(start_func), 1, arg);
	
	// insere a tarefa na fila de execucao
	queue_append((queue_t**)tsk_curr, (queue_t*)task);

	task->status = READY;
	id_last = task->id;

	return task->id;
}

//------------------------------------------------------------------------------
// Retorna o identificador da tarefa corrente (main deve ser 0)

int task_id ()
{
	return tsk_curr->id;
}

//------------------------------------------------------------------------------
// Termina a tarefa corrente com um valor de status de encerramento

void task_exit (int exit_code)
{
	#ifdef DEBUG
	printf("PPOS: task_exit > exiting task %d\n", tsk_curr->id);
	#endif /* !DEBUG */

	tsk_curr->status = TERMINATED;
	
	if (&tsk_main != tsk_curr)
		task_switch(&tsk_main);
}

//------------------------------------------------------------------------------
// Alterna a execução para a tarefa indicada

int task_switch (task_t *task)
{
	task_t *tmp;

	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot switch to a null task\n");
		return -1;
	}

	if (task->id == tsk_curr->id)
	{
		fprintf(stderr, "ERROR: cannot switch to the same task\n");
		return -2;
	}

	#ifdef DEBUG
	printf("PPOS: task_switch > switch from task %d to task %d\n", tsk_curr->id, task->id);
	#endif /* !DEBUG */
	
	task->status = RUNNING;
	if (tsk_curr->status != TERMINATED)
		tsk_curr->status = READY;
	
	tmp = tsk_curr;
	tsk_curr = task;

	swapcontext(&tmp->context, &task->context);

	return 0;
}
