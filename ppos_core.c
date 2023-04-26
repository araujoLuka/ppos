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
#include "ppos_data.h"
#include "queue.h"
#include <stdio.h>

task_t tsk_main, tsk_disp, *tsk_curr;
int id_new, id_last;
queue_t *q_tasks;

//------------------------------------------------------------------------------
// Retorna um ponteiro para a tarefa com maior prioridade e envelhece as demais

task_t *task_select_and_aging(task_t *q)
{
	task_t *t, *i;
	t = q;

	for (i = t->next; i != q; i = i->next)
	{
		if (t->p_din > i->p_din)
		{
			t->p_din += AGING_ALPHA;
			t = i;
		}
		else
			i->p_din += AGING_ALPHA;
	}

	return t;
}

//------------------------------------------------------------------------------
// Escalonador de tarefas; 
// retorna um ponteiro para a proxima tarefa a ser utilizada pelo processador
// caso nao exista tarefa retorna NULL

task_t *scheduler () 
{
	task_t *t;

	if (q_tasks == NULL)
		return NULL;

	// esquema de task aging
	t = task_select_and_aging((task_t*)q_tasks);

	#ifdef DEBUG
	printf("PPOS: scheduler > selected task %d (priority d:%d - e:%d)\n", t->id, t->p_din, t->p_sta);
	#endif

	t->p_din = task_getprio(t);

	switch (t->status)
	{
		case READY:
			return t;

		case RUNNING:
			fprintf(stderr, "ERROR: catched a running task in scheduler function\n");
			t->status = READY;
			q_tasks = q_tasks->next;
			break;

		case SUSPENDED:
			fprintf(stderr, "ERROR: catched a suspended task in scheduler function\n");
			t->status = READY;
			q_tasks = q_tasks->next;
			break;

		case TERMINATED:
			fprintf(stderr, "ERROR: catched a terminated task in tasks queue\n");
			queue_remove(&q_tasks, (queue_t*)t);
			break;	
	}

	return NULL;
}

//------------------------------------------------------------------------------
// Procedimento principal do despachante de tarefas

void dispatcher ()
{
	task_t *next;

	#ifdef DEBUG
	printf("PPOS: dispatcher > tasks dispatcher initialize\n");
	#endif
	
	// enquanto houverem tarefas de usuário
	while (queue_size(q_tasks) > 0) 
	{
		#ifdef DEBUG
		printf("PPOS: dispatcher > selecting next task\n");
		#endif

		// escolhe a proxima tarefa a executar
		next = scheduler();

		// escalonador escolher uma tarefa
		if (next != NULL)
		{
			// transfere o controle para a proxima tarefa
			task_switch(next);

			// voltando ao dispatcher, trata a tarefa de acordo com seu estado
			switch (next->status)
			{
				case TERMINATED:
					#ifdef DEBUG
					printf("PPOS: dispatcher > removed task %d from queue and freed memory\n", next->id);
					#endif
					queue_remove(&q_tasks, (queue_t*)next);
					free(next->context.uc_stack.ss_sp);
					break;

				case RUNNING:
					fprintf(stderr, "ERROR: task exited to dispatcher with RUNNING status\n");
					break;

				default:
					break;
			}
		}
	}
	
	#ifdef DEBUG
	printf("PPOS: dispatcher > tasks queue terminated\n");
	#endif
	task_exit(0);
}

//------------------------------------------------------------------------------
// Cria a estrutura de tarefa para o programa principal (main)

void main_init (task_t *m)
{
	#ifdef DEBUG
	printf("PPOS: main_init > created task struct to main task (id: %d)\n", ID_MAIN);
	#endif /* DEBUG */

	if (m == NULL)
		m = &tsk_main;

	ucontext_t a;
	getcontext(&a);

	tsk_main.id = ID_MAIN;
	tsk_main.context = a;
	tsk_main.next = NULL;
	tsk_main.prev = NULL;
	tsk_main.status = RUNNING;
	tsk_main.p_sta = DEFAULT_PRIORITY;
	tsk_main.p_din = task_getprio(m);
}

//------------------------------------------------------------------------------
// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void ppos_init ()
{
	#ifdef DEBUG
	printf("PPOS: ppos_init > system initialized\n");
	#endif /* DEBUG */

	main_init(&tsk_main);
	tsk_curr = &tsk_main;

	id_last = tsk_main.id;
	id_new = tsk_main.id + 1;

	setvbuf(stdout, 0, _IONBF, 0);

	task_init(&tsk_disp, dispatcher, NULL);
}

// Gerência de tarefas =========================================================



int task_create_stack(task_t *task)
{
	char *stack;

	if (task->id != ID_DISP)
		stack = malloc(STACKSIZE);
	else
	{
		char sttc[STACKSIZE];
		stack = sttc;
	}
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
		return 0;
	}

	return 1;
}

//------------------------------------------------------------------------------
// Inicializa uma nova tarefa. Retorna um ID> 0 ou erro.

int task_init (task_t *task, void (*start_func)(void *), void *arg)
{
	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot initialize a null task\n");
		return -1;
	}

	task->id = id_new++;
	#ifdef DEBUG
	printf("PPOS: task_init > task %d created by task %d (func %p)\n", task->id, task_id(), start_func);
	#endif /* DEBUG */

	// salva as informacoes do contexto atual para
	// ser usado como base do novo contexto
	getcontext(&task->context);

	// aloca espaco para a stack do contexto a ser criado
	if (!task_create_stack(task))
		return -2;

	// cria o contexto da tarefa
	makecontext(&task->context, (void*)(start_func), 1, arg);
	
	if (task->id != ID_DISP)
		// insere a tarefa na fila de execucao
		queue_append(&q_tasks, (queue_t*)task);

	task->status = READY;
	task->p_sta = DEFAULT_PRIORITY;
	task->p_din = task_getprio(task);
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
	printf("PPOS: task_exit > exiting task %d\n", task_id());
	#endif /* DEBUG */

	tsk_curr->status = TERMINATED;
	
	switch (task_id())
	{
		case ID_DISP:
			exit(0);
			break;

		default:
			task_switch(&tsk_disp);
	}
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

	if (task->id == task_id())
	{
		fprintf(stderr, "ERROR: cannot switch to the same task\n");
		return -2;
	}

	#ifdef DEBUG
	printf("PPOS: task_switch > switch from task %d to task %d\n", task_id(), task->id);
	#endif /* DEBUG */
	
	task->status = RUNNING;

	if (tsk_curr->status != TERMINATED)
		tsk_curr->status = READY;
	
	tmp = tsk_curr;
	tsk_curr = task;

	swapcontext(&tmp->context, &task->context);

	return 0;
}

// operações de escalonamento ==================================================

//------------------------------------------------------------------------------
// A tarefa atual libera o processador para outra tarefa

void task_yield ()
{
	q_tasks = q_tasks->next;
	task_switch(&tsk_disp);
}

//------------------------------------------------------------------------------
// define a prioridade estática de uma tarefa (ou a tarefa atual)

void task_setprio (task_t *task, int prio)
{
	if (prio > MAX_PRIORITY)
		prio = MAX_PRIORITY;
	
	if (prio < MIN_PRIORITY)
		prio = MIN_PRIORITY;

	if (task == NULL)
		task = tsk_curr;
	
	task->p_sta = prio;
	if (task->p_din < prio)
		task->p_din = prio;
}

//------------------------------------------------------------------------------
// retorna a prioridade estática de uma tarefa (ou a tarefa atual)

int task_getprio (task_t *task)
{
	if (task == NULL)
		task = tsk_curr;
		
	return task->p_sta;
}
