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
#include "queue.h"
#include <unistd.h>

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
void task_release (task_t **queue);

// Start

// Inicializador do sistema ====================================================

//------------------------------------------------------------------------------
// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void ppos_init ()
{
	#ifdef DEBUG
	printf("PPOS: ppos_init > system initialized\n");
	#endif /* DEBUG */

	tsk.ready = NULL;
    tsk.sleeping = NULL;
    tsk.current = NULL;
	main_init(&tsk.main);
	tsk.current = &tsk.main;

	tsk.id_new = tsk.main.id + 1;

	setvbuf(stdout, 0, _IONBF, 0);

	task_init(&tsk.dispatcher, dispatcher, NULL);
	tsk.dispatcher.quantum = 99;
	tsk.dispatcher.user_task = 0;

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
		m = &tsk.main;

	m->id = ID_MAIN;
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
    m->wake_time = 0;

	// insere a tarefa na fila de execucao
	queue_append(&tsk.ready, (queue_t*)m);
}

// Operações de gestão do tempo ================================================

//------------------------------------------------------------------------------
// Retorna o relógio atual (em milisegundos)

unsigned int systime ()
{
	return tmr.sys_timer;
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
// Tratador de ticks

void tick_handler ()
{
	if (tmr.kernel_lock)
		return;

	if (tsk.current->quantum == 0)
	{
		#ifdef DEBUG
		printf("PPOS: tick_handler > task %d finished quantum\n", task_id());
		#endif /* DEBUG */
		task_yield();
	}

	tmr.sys_timer++;
	tsk.current->proc_time++;
	tsk.current->quantum -= tsk.current->user_task;
}

//------------------------------------------------------------------------------
// Suspende a tarefa corrente por t milissegundos

void task_sleep (int t)
{
	if (t <= 0)
		return;

	tsk.current->wake_time = systime() + t;

	task_suspend((task_t **)&tsk.sleeping);
}

//------------------------------------------------------------------------------
// Acorda as tarefas da fila de tarefas dormindo que podem ser acordadas
// - Retorna o menor tempo que falta para uma tarefa acordar dentre todas
// - Caso a fila esteja vazia retorna 0

unsigned int task_awake()
{
	task_t *aux1, *aux2;
	int tam, tmp;
	unsigned int less;

	tam = queue_size(tsk.sleeping);
	less = 1000;

	if (tam == 0)
		return 0;

	aux1 = (task_t *)tsk.sleeping;
	aux2 = aux1->next;

	for (int i=0; i < tam; i++)
	{
		tmp = aux1->wake_time - systime() - 1;
		if (tmp < 0)
		{
			task_resume(aux1, (task_t **)&tsk.sleeping);
			tam--;
		}
		else
		{
			if (tmp < less)
				less = tmp;
		}

		aux1 = aux2;
		aux2 = aux1->next;
	}

	return less;
}

// Controle de execucao de tarefas =============================================

//------------------------------------------------------------------------------
// Procedimento principal do despachante de tarefas

void dispatcher ()
{
	task_t *next;
	unsigned int less_wake_time;

	#ifdef DEBUG
	printf("PPOS: dispatcher > tasks dispatcher initialize\n");
	#endif
	
	while (1) 
	{
		less_wake_time = task_awake();
			
		// enquanto houverem tarefas de usuário
		if (queue_size(tsk.ready) > 0) 
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
						queue_remove(&tsk.ready, (queue_t*)next);
						free(next->context.uc_stack.ss_sp);
						break;

					case RUNNING:
						fprintf(stderr, "ERROR: task exited to dispatcher with RUNNING status\n");
						exit(141);
						break;

					default:
						break;
				}
			}
		}
		else
		{
			if (queue_size(tsk.sleeping) == 0)
				break;
			// faz o dispatcher dormir para aliviar o processamento
			usleep(less_wake_time);
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

	if (tsk.ready == NULL)
		return NULL;

	// esquema de task aging
	t = task_select_and_aging((task_t*)tsk.ready);

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
			tsk.ready = tsk.ready->next;
			break;

		case SUSPENDED:
			fprintf(stderr, "ERROR: catched a suspended task in scheduler function\n");
			t->status = READY;
			tsk.ready = tsk.ready->next;
			break;

		case TERMINATED:
			fprintf(stderr, "ERROR: catched a terminated task in tasks queue\n");
			queue_remove(&tsk.ready, (queue_t*)t);
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
	tmr.kernel_lock = 1;

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
	
    task->prev = NULL;
    task->next = NULL;
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
    task->wake_time = 0;

	if (task->id != ID_DISP)
		// insere a tarefa na fila de execucao
		queue_append(&tsk.ready, (queue_t*)task);

	tmr.kernel_lock = 0;
	return task->id;
}

//------------------------------------------------------------------------------
// Aloca memoria para o contexto da tarefa

int task_create_stack (task_t *task)
{
	char *stack;

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
		return 0;
	}

	return 1;
}

//------------------------------------------------------------------------------
// Retorna o identificador da tarefa corrente (main deve ser 0)

int task_id ()
{
	return tsk.current->id;
}

//------------------------------------------------------------------------------
// Termina a tarefa corrente com um valor de status de encerramento

void task_exit (int exit_code)
{
	tmr.kernel_lock = 1;
	#ifdef DEBUG
	printf("PPOS: task_exit > exiting task %d\n", task_id());
	#endif /* DEBUG */

	tsk.current->status = TERMINATED;
	tsk.current->exec_time = systime() - tsk.current->exec_time;
	printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",
				task_id(),
				tsk.current->exec_time,
				tsk.current->proc_time,
				tsk.current->activations
	);
	tsk.current->exit_code = exit_code;
	
	task_release(&tsk.current->waiting);
	
	tmr.kernel_lock = 0;

	switch (task_id())
	{
		case ID_DISP:
			free(tsk.dispatcher.context.uc_stack.ss_sp);
			exit(0);
			break;

		default:
			task_switch(&tsk.dispatcher);
	}
}

//------------------------------------------------------------------------------
// Libera todas as tarefas da fila 'queue' com task_resume

void task_release (task_t **queue)
{
	task_t *aux, *next;

	aux = *queue;
	if (aux == NULL)
		return;

	for (next = aux->next; aux != next; aux = next, next = aux->next) 
		task_resume(aux, queue);

	if (aux != NULL)
		task_resume(aux, queue);
}

//------------------------------------------------------------------------------
// Alterna a execução para a tarefa indicada

int task_switch (task_t *task)
{
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
	
	tmr.kernel_lock = 1;

	task->status = RUNNING;
	
	old = tsk.current;
	tsk.current = task;

	if (old->status == RUNNING)
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
	task_t *tmp;
	tmp = tsk.current;

	#ifdef DEBUG
	printf("PPOS: task_suspend > suspending task %d\n", task_id());
	#endif /* DEBUG */

	tmr.kernel_lock = 1;

	// tenta remover tarefa da fila de prontas
	// tratamento para caso a tarefa nao pertenca a fila dentro da funcao
	queue_remove(&tsk.ready, (queue_t *)tmp);

	// ajusta o estado da tarefa
	tmp->status = SUSPENDED;
	
	// insere a tarefa suspensa na fila queue
	if (queue != NULL)
		queue_append((queue_t **)queue, (queue_t *)tmp);

	tmr.kernel_lock = 0;
	task_switch(&tsk.dispatcher);
}

//------------------------------------------------------------------------------
// Acorda a tarefa indicada, trasferindo-a da fila "queue" para a fila de prontas

void task_resume (task_t *task, task_t **queue)
{
	if (queue != NULL)
		queue_remove((queue_t **)queue, (queue_t *)task);

	task->status = READY;

	tmr.kernel_lock = 1;

	queue_append(&tsk.ready, (queue_t *)task);

	tmr.kernel_lock = 0;
}

// Operações de escalonamento ==================================================

//------------------------------------------------------------------------------
// A tarefa atual libera o processador para outra tarefa

void task_yield ()
{
	tmr.kernel_lock = 1;
	tsk.ready = tsk.ready->next;
	tmr.kernel_lock = 0;
	task_switch(&tsk.dispatcher);
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
		task = tsk.current;
	
	task->p_sta = prio;
	if (task->p_din < prio)
		task->p_din = prio;
}

//------------------------------------------------------------------------------
// Retorna a prioridade estática de uma tarefa (ou a tarefa atual)

int task_getprio (task_t *task)
{
	if (task == NULL)
		task = tsk.current;
		
	return task->p_sta;
}

// Operações de sincronização ==================================================

//------------------------------------------------------------------------------

// A tarefa corrente aguarda o encerramento de outra task
int task_wait (task_t *task) 
{
	if (task == NULL)
	{
		return -1;
	}

	if (task->status == TERMINATED)
	{
		return -1;
	}

	#ifdef DEBUG
	printf("PPOS: task_wait > setting task %d to wait task %d\n", task_id(), task->id);
	#endif /* DEBUG */
	
	task_suspend(&task->waiting);
	
	#ifdef DEBUG
	printf("PPOS: task_wait > task %d ended with exit_code %d\n", task->id, task->exit_code);
	#endif /* DEBUG */
	
	return task->exit_code;
}

// inicializa um semáforo com valor inicial "value"
int sem_init (semaphore_t *s, int value) 
{
    tmr.kernel_lock = 1;

    s->init = value;
    s->counter = value;
    s->tasks = NULL;
    s->in_use = 0;

    tmr.kernel_lock = 0;
    return 0;
}

// requisita o semáforo
int sem_down (semaphore_t *s)
{
    tmr.kernel_lock = 1;

    if (s == NULL)
    {
        tmr.kernel_lock = 0;
        return -1;
    }

    while (s->in_use);
    s->in_use = 1;
    
    if (s->counter <= 0)
    {
        s->in_use = 0;
        task_suspend((task_t**)&s->tasks);
    }

    if (s == NULL)
        return -1;

    s->counter--;
    s->in_use = 0;
    tmr.kernel_lock = 0;

    return 0;
}

// libera o semáforo
int sem_up (semaphore_t *s)
{
    if (s == NULL)
    {
        return -1;
    }

    if (s->init < ++s->counter)
        s->counter = s->init;

    if (s->tasks != NULL)
        task_resume((task_t*)s->tasks, (task_t**)&s->tasks);

    return 0;
}

// "destroi" o semáforo, liberando as tarefas bloqueadas
int sem_destroy (semaphore_t *s)
{
    task_t *t;

    tmr.kernel_lock = 1;

    for (t = (task_t*)s->tasks; queue_size(s->tasks) > 0; t = (task_t*)s->tasks)
    {
        task_resume(t, (task_t **)&s->tasks);
        tmr.kernel_lock = 1;
    }

    s = NULL;

    tmr.kernel_lock = 0;

    return 0;
}
