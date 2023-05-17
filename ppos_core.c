// Lucas Correia de Araujo - GRR 20206150
// PingPongOS - PingPong Operating System
// 'ppos_core.c'
// -- Codigo nucleo do sistema operacional
//
// Disciplina: Sistema Operacionais (CI1215)
// Professor: Carlos A. Maziero, DINF UFPR
//
// Abril de 2023
// Funcoes e operacoes internas do sistema

#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"
#include <stdio.h>

// Variaveis globais
manager_t tm;
int id_new, id_last;
struct sigaction action;
struct itimerval timer;
int kernel_lock = 0;
unsigned int sys_timer = 0;

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

	if (tm.q_tasks == NULL)
		return NULL;

	// esquema de task aging
	t = task_select_and_aging((task_t*)tm.q_tasks);

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
			tm.q_tasks = tm.q_tasks->next;
			break;

		case SUSPENDED:
			fprintf(stderr, "ERROR: catched a suspended task in scheduler function\n");
			t->status = READY;
			tm.q_tasks = tm.q_tasks->next;
			break;

		case TERMINATED:
			fprintf(stderr, "ERROR: catched a terminated task in tasks queue\n");
			queue_remove(&tm.q_tasks, (queue_t*)t);
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
	while (queue_size(tm.q_tasks) > 0) 
	{
		#ifdef DEBUG
		printf("PPOS: dispatcher > selecting next task\n");
		#endif

		// escolhe a proxima tarefa a executar
		next = scheduler();

		// escalonador escolher uma tarefa
		if (next != NULL)
		{
			// reseta o quantum da terafa a receber o processador
			next->quantum = DEFAULT_QUANTUM;

			// transfere o controle para a proxima tarefa
			task_switch(next);

			// voltando ao dispatcher, trata a tarefa de acordo com seu estado
			switch (next->status)
			{
				case TERMINATED:
					#ifdef DEBUG
					printf("PPOS: dispatcher > removed task %d from queue and freed memory\n", next->id);
					#endif
					queue_remove(&tm.q_tasks, (queue_t*)next);
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

unsigned int systime()
{
	return sys_timer;
}

//------------------------------------------------------------------------------
// Tratador de ticks

void tick_handler ()
{
	if (kernel_lock)
		return;

	if (tm.tsk_curr->quantum <= 0)
	{
		#ifdef DEBUG
		printf("PPOS: tick_handler > task %d finished quantum\n", task_id());
		#endif /* DEBUG */
		task_yield();
	}

	sys_timer++;
	tm.tsk_curr->proc_time++;
	tm.tsk_curr->quantum -= tm.tsk_curr->user_task;
}

//------------------------------------------------------------------------------
// Inicializador para programar os ticks do sistema em 1000ms

void timer_init ()
{
	#ifdef DEBUG
	printf("PPOS: timer_init > creating tick interruptions with %d ms freq\n", TICK_FREQ_IN_MS / 1000);
	#endif /* DEBUG */

	// registra a ação para o sinal de timer SIGALRM (sinal do timer)
	action.sa_handler = tick_handler ;
	sigemptyset (&action.sa_mask) ;
	action.sa_flags = 0 ;
	if (sigaction (SIGALRM, &action, 0) < 0)
	{
		perror ("Erro em sigaction: ") ;
		exit (1) ;
	}

	// ajusta valores do temporizador
	timer.it_value.tv_usec = TICK_FREQ_IN_MS * 1000 ;	// primeiro disparo, em micro-segundos
	timer.it_value.tv_sec  = 0 ;			// primeiro disparo, em segundos
	timer.it_interval.tv_usec = TICK_FREQ_IN_MS * 1000;	// disparos subsequentes, em micro-segundos
	timer.it_interval.tv_sec  = 0 ;		// disparos subsequentes, em segundos

	// arma o temporizador ITIMER_REAL
	if (setitimer (ITIMER_REAL, &timer, 0) < 0)
	{
		perror ("Erro em setitimer: ") ;
		exit (1) ;
	}
}

//------------------------------------------------------------------------------
// Cria a estrutura de tarefa para o programa principal (main)

void main_init (task_t *m)
{
	#ifdef DEBUG
	printf("PPOS: main_init > created task struct to main task (id: %d)\n", ID_MAIN);
	#endif /* DEBUG */

	if (m == NULL)
		m = &tm.tsk_main;

	ucontext_t a;
	getcontext(&a);

	tm.tsk_main.id = ID_MAIN;
	tm.tsk_main.context = a;
	tm.tsk_main.next = NULL;
	tm.tsk_main.prev = NULL;
	tm.tsk_main.status = RUNNING;
	tm.tsk_main.p_sta = DEFAULT_PRIORITY;
	tm.tsk_main.p_din = task_getprio(m);
	tm.tsk_main.quantum = DEFAULT_QUANTUM;
	tm.tsk_main.user_task = 1;
	tm.tsk_main.proc_time = 0;
	tm.tsk_main.exec_time = systime();
	tm.tsk_main.activations = 1;
}

//------------------------------------------------------------------------------
// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void ppos_init ()
{
	#ifdef DEBUG
	printf("PPOS: ppos_init > system initialized\n");
	#endif /* DEBUG */

	main_init(&tm.tsk_main);
	tm.tsk_curr = &tm.tsk_main;

	id_last = tm.tsk_main.id;
	id_new = tm.tsk_main.id + 1;

	setvbuf(stdout, 0, _IONBF, 0);

	timer_init();

	task_init(&tm.tsk_disp, dispatcher, NULL);
	tm.tsk_disp.quantum = 99;
	tm.tsk_disp.user_task = 0;
}

// Gerência de tarefas =========================================================

//------------------------------------------------------------------------------
// Aloca memoria para o contexto da tarefa

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
		queue_append(&tm.q_tasks, (queue_t*)task);

	task->status = READY;
	task->p_sta = DEFAULT_PRIORITY;
	task->p_din = task_getprio(task);
	task->quantum = DEFAULT_QUANTUM;
	task->user_task = 1;
	task->proc_time = 0;
	task->exec_time = systime();
	task->activations = 0;

	id_last = task->id;

	return task->id;
}

//------------------------------------------------------------------------------
// Retorna o identificador da tarefa corrente (main deve ser 0)

int task_id ()
{
	return tm.tsk_curr->id;
}

//------------------------------------------------------------------------------
// Termina a tarefa corrente com um valor de status de encerramento

void task_exit (int exit_code)
{
	#ifdef DEBUG
	printf("PPOS: task_exit > exiting task %d\n", task_id());
	#endif /* DEBUG */

	tm.tsk_curr->status = TERMINATED;
	tm.tsk_curr->exec_time = systime() - tm.tsk_curr->exec_time;
	printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",
				task_id(),
				tm.tsk_curr->exec_time,
				tm.tsk_curr->proc_time,
				tm.tsk_curr->activations
	);
	
	switch (task_id())
	{
		case ID_DISP:
			exit(0);
			break;

		default:
			task_switch(&tm.tsk_disp);
	}
}

//------------------------------------------------------------------------------
// Alterna a execução para a tarefa indicada

int task_switch (task_t *task)
{
	kernel_lock = 1;
	task_t *old;

	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot switch to a null task\n");
		kernel_lock = 0;
		return -1;
	}

	if (task->id == task_id())
	{
		fprintf(stderr, "ERROR: cannot switch to the same task\n");
		kernel_lock = 0;
		return -2;
	}

	#ifdef DEBUG
	printf("PPOS: task_switch > switch from task %d to task %d\n", task_id(), task->id);
	#endif /* DEBUG */
	
	task->status = RUNNING;
	
	old = tm.tsk_curr;
	tm.tsk_curr = task;

	if (old->status != TERMINATED)
		old->status = READY;

	task->activations++;
	kernel_lock = 0;
	swapcontext(&old->context, &task->context);

	return 0;
}

// operações de escalonamento ==================================================

//------------------------------------------------------------------------------
// A tarefa atual libera o processador para outra tarefa

void task_yield ()
{
	kernel_lock = 1;
	tm.q_tasks = tm.q_tasks->next;
	task_switch(&tm.tsk_disp);
	kernel_lock = 0;
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
		task = tm.tsk_curr;
	
	task->p_sta = prio;
	if (task->p_din < prio)
		task->p_din = prio;
}

//------------------------------------------------------------------------------
// retorna a prioridade estática de uma tarefa (ou a tarefa atual)

int task_getprio (task_t *task)
{
	if (task == NULL)
		task = tm.tsk_curr;
		
	return task->p_sta;
}
