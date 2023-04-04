// PingPongOS - PingPong Operating System
// -- Biblioteca de Filas
//
// Autor: Lucas Correia de Araujo
//
// Disciplina: Sistema Operacionais (CI1215)
// Prof. Carlos A. Maziero, DINF UFPR
//
// Abril de 2023
// Implementacao de operações em uma fila genérica.

#include "queue.h"
#include <stdio.h>

//------------------------------------------------------------------------------
// Verifica se o elemento eh isolado
// Condicoes para verdadeiro:
// - ponteiros de proximo e anterior sao nulos
// Retorno: 1 se isolado, 0 caso contrario

int elem_isolated(queue_t *elem){
	if (!elem->prev && !elem->next)
		return 1;
	
	return 0;
}

//------------------------------------------------------------------------------
// Verifica se o elemento pertence a fila
// Retorno: 1 se pertence, 0 caso contrario

int elem_in_queue (queue_t *elem, queue_t *queue){
	queue_t *aux;
	int size;

	aux = elem;
	size = queue_size(queue);
	
	for (int i = 0; i < size; i++){
		if (aux == queue)
			return 1;

		aux = aux->next;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Informa se a fila esta vazia
// Retorno: 1 se vazia, 0 caso contrario

int queue_empty(queue_t *queue){
	if (!queue_size(queue))
		return 1;

	return 0;
}

//------------------------------------------------------------------------------
// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila

int queue_size(queue_t *queue){
	int i;
	queue_t *aux;
	
	aux = queue;

	if (!aux)
		return 0;
	
	aux = aux->next;
	for (i = 1; aux != queue; i++){
		aux = aux->next;
	}

	return i;
}

//------------------------------------------------------------------------------
// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca. Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir

void queue_print(char *name, queue_t *queue, void (*print_elem)(void *)){
	queue_t *aux;

	// Imprime a string de parametro e o indicador de inicio da fila
	printf("%s: [", name);
	
	// Imprime elementos se fila nao for vazia
	if (!queue_empty(queue)){
		aux = queue;
		print_elem(aux);
		aux = aux->next;

		while (aux != queue){
			printf(" ");
			print_elem(aux);
			aux = aux->next;
		}
	}

	// Imprime inidicador de final da fila
	printf("]\n");
}

//------------------------------------------------------------------------------
// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila
// Retorno: 0 se sucesso, <0 se ocorreu algum erro

int queue_append(queue_t **queue, queue_t *elem){
	queue_t *aux;
	int size;

	// Verifica se fila existe
	if (!queue){
		return -1;
	}

	// Verifica se elemento existe
	if (!elem){
		return -2;
	}

	// Verifica se elemento nao eh isolado
	if (!elem_isolated(elem)){
		return -3;
	}

	size = queue_size(*queue);
	switch (size){
		case 0:
			*queue = elem;
			elem->next = elem;
			elem->prev = elem;
			break;

		default:
			aux = (*queue)->prev;
			aux->next->prev = elem;
			aux->next = elem;
			elem->prev = aux;
			elem->next = *queue;
			break;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: 0 se sucesso, <0 se ocorreu algum erro

int queue_remove(queue_t **queue, queue_t *elem){
	// Verifica se fila existe
	if (!queue){
		return -1;
	}

	// Verifica se elemento existe
	if (!elem){
		return -2;
	}

	// Verifica se a fila esta vazia
	if (queue_empty(*queue)){
		return -4;
	}

	// Verifica se elemento eh isolado
	if (elem_isolated(elem)){
		return -5;
	}

	// Verifica se o elemento pertence a lista
	if (!elem_in_queue(elem, *queue)){
		return -6;
	}

	// Ajusta ponteiros dos vizinhos para retirar elemento
	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;

	// Verifica se o elemento removido eh o inicio da fila
	// Se verdadeiro altera o inicio da fila para o proximo elemento
	if (elem == *queue)
		*queue = elem->next;

	// Isola o elemento removido
	elem->next = NULL;
	elem->prev = NULL;

	// Verifica se o inicio da fila esta isolado
	// Se verdadeiro, ajusta o inicio da fila para NULL, pois todos 
	// os elementos foram removidos
	if (elem_isolated(*queue))
		*queue = NULL;

	return 0;
}
