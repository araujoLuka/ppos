// Lucas Correia de Araujo - GRR 20206150
// PingPongOS - PingPong Operating System
// 'ppos_core.c'
// -- Codigo nucleo do sistema operacional
//
// Disciplina: Sistema Operacionais (CI1215)
// Professor: Carlos A. Maziero, DINF UFPR
//
// Maio de 2023
// Funcoes e operacoes internas do sistema

#include "ppos.h"
#include "ppos_data.h"

// Estruturas globais
tsk_manager_t tsk;
tmr_manager_t tmr;

// Prototipos para funcoes privadas
task_t *task_select_and_aging (task_t *q);
task_t *scheduler ();
void dispatcher ();
void tick_handler ();
void timer_init ();
void main_init (task_t *m);
int task_create_stack (task_t *task);
void task_wake_queue (task_t **queue);

// Start

// Inicializador do sistema ====================================================

//------------------------------------------------------------------------------
// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void ppos_init ()
{
	#ifdef DEBUG
	printf("PPOS: ppos_init > system initialized\n");
	#endif /* DEBUG */

	main_init(&tsk.t_main);
	tsk.t_curr = &tsk.t_main;

	tsk.id_last = tsk.t_main.id;
	tsk.id_new = tsk.t_main.id + 1;

	setvbuf(stdout, 0, _IONBF, 0);

	task_init(&tsk.t_disp, dispatcher, NULL);
	tsk.t_disp.quantum = 99;
	tsk.t_disp.user_task = 0;

	timer_init();
}

//------------------------------------------------------------------------------
// Cria a estrutura de tarefa para o programa principal (main)

void main_init (task_t *m)
{
	#ifdef DEBUG
	printf("PPOS: main_init > created task struct to main task (id: %d)\n", ID_MAIN);
	#endif /* DEBUG */

	if (m == NULL)
		m = &tsk.t_main;

	ucontext_t a;
	getcontext(&a);

	m->id = ID_MAIN;
	m->context = a;
	m->next = NULL;
	m->prev = NULL;
	m->status = RUNNING;
	m->p_sta = DEFAULT_PRIORITY;
	m->p_din = task_getprio(m);
	m->quantum = DEFAULT_QUANTUM;
	m->user_task = 1;
	m->proc_time = 0;
	m->exec_time = systime();
	m->activations = 1;
	m->waiting = NULL;
	m->exit_code = 0;
	// insere a tarefa na fila de execucao
	queue_append(&tsk.q_tasks, (queue_t*)m);
}

//------------------------------------------------------------------------------
// Programa os ticks de interrupcoes do sistema no tempo dado por TICK_FREQ_IN_MS

void timer_init ()
{
	#ifdef DEBUG
	printf("PPOS: timer_init > creating tick interruptions with %d ms freq\n", TICK_FREQ_IN_MS / 1000);
	#endif /* DEBUG */

	// registra a ação para o sinal de timer SIGALRM (sinal do timer)
	tmr.int_action.sa_handler = tick_handler ;
	sigemptyset (&tmr.int_action.sa_mask) ;
	tmr.int_action.sa_flags = 0 ;
	if (sigaction (SIGALRM, &tmr.int_action, 0) < 0)
	{
		perror ("Erro em sigaction: ") ;
		exit (1) ;
	}

	// ajusta valores do temporizador
	tmr.int_timer.it_value.tv_usec = TICK_FREQ_IN_MS * 1000 ;	// primeiro disparo, em micro-segundos
	tmr.int_timer.it_value.tv_sec  = 0 ;			// primeiro disparo, em segundos
	tmr.int_timer.it_interval.tv_usec = TICK_FREQ_IN_MS * 1000;	// disparos subsequentes, em micro-segundos
	tmr.int_timer.it_interval.tv_sec  = 0 ;		// disparos subsequentes, em segundos

	// arma o temporizador ITIMER_REAL
	if (setitimer (ITIMER_REAL, &tmr.int_timer, 0) < 0)
	{
		perror ("Erro em setitimer: ") ;
		exit (1) ;
	}
}

//------------------------------------------------------------------------------
// Retorna o relógio atual (em milisegundos)

unsigned int systime ()
{
	return tmr.sys_timer;
}

//------------------------------------------------------------------------------
// Tratador de ticks

void tick_handler ()
{
	if (tmr.kernel_lock)
		return;

	if (tsk.t_curr->quantum <= 0)
	{
		#ifdef DEBUG
		printf("PPOS: tick_handler > task %d finished quantum\n", task_id());
		#endif /* DEBUG */
		task_yield();
	}

	tmr.sys_timer++;
	tsk.t_curr->proc_time++;
	tsk.t_curr->quantum -= tsk.t_curr->user_task;
}
// Controle de execucao de tarefas =============================================

//------------------------------------------------------------------------------
// Procedimento principal do despachante de tarefas

void dispatcher ()
{
	task_t *next;

	#ifdef DEBUG
	printf("PPOS: dispatcher > tasks dispatcher initialize\n");
	#endif
	
	// enquanto houverem tarefas de usuário
	while (queue_size(tsk.q_tasks) > 0) 
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
					queue_remove(&tsk.q_tasks, (queue_t*)next);
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
// Escalonador de tarefas; 
// retorna um ponteiro para a proxima tarefa a ser utilizada pelo processador
// caso nao exista tarefa retorna NULL

task_t *scheduler () 
{
	task_t *t;

	if (tsk.q_tasks == NULL)
		return NULL;

	// esquema de task aging
	t = task_select_and_aging((task_t*)tsk.q_tasks);

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
			tsk.q_tasks = tsk.q_tasks->next;
			break;

		case SUSPENDED:
			fprintf(stderr, "ERROR: catched a suspended task in scheduler function\n");
			t->status = READY;
			tsk.q_tasks = tsk.q_tasks->next;
			break;

		case TERMINATED:
			fprintf(stderr, "ERROR: catched a terminated task in tasks queue\n");
			queue_remove(&tsk.q_tasks, (queue_t*)t);
			break;	
	}

	return NULL;
}

//------------------------------------------------------------------------------
// Retorna um ponteiro para a tarefa com maior prioridade e envelhece as demais

task_t *task_select_and_aging (task_t *q)
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

// Gerência de tarefas =========================================================

//------------------------------------------------------------------------------
// Inicializa uma nova tarefa. Retorna um ID> 0 ou erro.

int task_init (task_t *task, void (*start_func)(void *), void *arg)
{
	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot initialize a null task\n");
		return -1;
	}

	task->id = tsk.id_new++;
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
		queue_append(&tsk.q_tasks, (queue_t*)task);

	task->status = READY;
	task->p_sta = DEFAULT_PRIORITY;
	task->p_din = task_getprio(task);
	task->quantum = DEFAULT_QUANTUM;
	task->user_task = 1;
	task->proc_time = 0;
	task->exec_time = systime();
	task->activations = 0;
	task->waiting = NULL;
	task->exit_code = 0;

	tsk.id_last = task->id;

	return task->id;
}

//------------------------------------------------------------------------------
// Aloca memoria para o contexto da tarefa

int task_create_stack (task_t *task)
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
// Retorna o identificador da tarefa corrente (main deve ser 0)

int task_id ()
{
	return tsk.t_curr->id;
}

//------------------------------------------------------------------------------
// Termina a tarefa corrente com um valor de status de encerramento

void task_exit (int exit_code)
{
	#ifdef DEBUG
	printf("PPOS: task_exit > exiting task %d\n", task_id());
	#endif /* DEBUG */

	tsk.t_curr->status = TERMINATED;
	tsk.t_curr->exec_time = systime() - tsk.t_curr->exec_time;
	printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",
				task_id(),
				tsk.t_curr->exec_time,
				tsk.t_curr->proc_time,
				tsk.t_curr->activations
	);
	tsk.t_curr->exit_code = exit_code;
	
	task_wake_queue(&tsk.t_curr->waiting);
	
	switch (task_id())
	{
		case ID_DISP:
			exit(0);
			break;

		default:
			task_switch(&tsk.t_disp);
	}
}

void task_wake_queue (task_t **queue)
{
	task_t *aux, *next;

	aux = *queue;
	if (aux == NULL)
		return;

	for (next = aux->next; aux != next; aux = next, next = aux->next) 
	{
		aux->status = READY;
		queue_remove((queue_t **)queue, (queue_t *)aux);
		queue_append(&tsk.q_tasks, (queue_t *)aux);
	}

	if (aux != NULL)
	{
		aux->status = READY;
		queue_remove((queue_t **)queue, (queue_t *)aux);
		queue_append(&tsk.q_tasks, (queue_t *)aux);
	}
}

//------------------------------------------------------------------------------
// Alterna a execução para a tarefa indicada

int task_switch (task_t *task)
{
	tmr.kernel_lock = 1;
	task_t *old;

	if (task == NULL)
	{
		fprintf(stderr, "ERROR: cannot switch to a null task\n");
		tmr.kernel_lock = 0;
		return -1;
	}

	if (task->id == task_id())
	{
		fprintf(stderr, "ERROR: cannot switch to the same task\n");
		tmr.kernel_lock = 0;
		return -2;
	}

	#ifdef DEBUG
	printf("PPOS: task_switch > switch from task %d to task %d\n", task_id(), task->id);
	#endif /* DEBUG */
	
	task->status = RUNNING;
	
	old = tsk.t_curr;
	tsk.t_curr = task;

	if (old->status != TERMINATED && old->status != SUSPENDED)
		old->status = READY;

	task->activations++;
	tmr.kernel_lock = 0;
	swapcontext(&old->context, &task->context);

	return 0;
}

//------------------------------------------------------------------------------
// Suspende a tarefa atual, transferindo-a da fila de prontas para a fila "queue"

void task_suspend (task_t **queue) 
{
	tmr.kernel_lock = 1;
	if (tsk.t_curr->status == READY)
		queue_remove(&tsk.q_tasks, (queue_t *)tsk.t_curr);

	tsk.t_curr->status = SUSPENDED;
	
	if (queue != NULL)
		queue_append((queue_t **)queue, (queue_t *)tsk.t_curr);

	task_yield();
}

//------------------------------------------------------------------------------
// Acorda a tarefa indicada, trasferindo-a da fila "queue" para a fila de prontas

void task_resume (task_t *task, task_t **queue)
{
	if (queue != NULL)
		queue_remove((queue_t **)queue, (queue_t *)task);

	task->status = READY;

	queue_append(&tsk.q_tasks, (queue_t *)task);
}

// Operações de escalonamento ==================================================

//------------------------------------------------------------------------------
// A tarefa atual libera o processador para outra tarefa

void task_yield ()
{
	tmr.kernel_lock = 1;
	tsk.q_tasks = tsk.q_tasks->next;
	task_switch(&tsk.t_disp);
	tmr.kernel_lock = 0;
}

//------------------------------------------------------------------------------
// Define a prioridade estática de uma tarefa (ou a tarefa atual)

void task_setprio (task_t *task, int prio)
{
	if (prio > MAX_PRIORITY)
		prio = MAX_PRIORITY;
	
	if (prio < MIN_PRIORITY)
		prio = MIN_PRIORITY;

	if (task == NULL)
		task = tsk.t_curr;
	
	task->p_sta = prio;
	if (task->p_din < prio)
		task->p_din = prio;
}

//------------------------------------------------------------------------------
// Retorna a prioridade estática de uma tarefa (ou a tarefa atual)

int task_getprio (task_t *task)
{
	if (task == NULL)
		task = tsk.t_curr;
		
	return task->p_sta;
}

// Operações de sincronização ==================================================

//------------------------------------------------------------------------------
// A tarefa corrente aguarda o encerramento de outra task

int task_wait (task_t *task) 
{
	tmr.kernel_lock = 1;
	if (task == NULL)
		return -1;

	if (task->status == TERMINATED)
		return -1;

	task_suspend(&task->waiting);
	
	return task->exit_code;
}
