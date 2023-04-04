#include "queue.h"
#include <stdio.h>

int elem_isolated(queue_t *elem){
	if (!elem->prev && !elem->next)
		return 1;
	
	return 0;
}

int elem_in_queue (queue_t *elem, queue_t *queue){
	queue_t *aux;
	int size;

	aux = elem;
	size = queue_size(queue);
	
	for (int i = 0; i < size; i++) {
		if (aux == queue)
			return 1;

		aux = aux->next;
	}

	return 0;
}

int queue_empty(queue_t *queue){
	if (!queue_size(queue))
		return 1;

	return 0;
}

int queue_size(queue_t *queue){
	int i;
	queue_t *aux;
	
	aux = queue;

	if (!aux)
		return 0;
	
	aux = aux->next;
	for (i = 1; aux != queue; i++) {
		aux = aux->next;
	}

	return i;
}

void queue_print(char *name, queue_t *queue, void (*print_elem)(void *)){
	queue_t *aux;

	aux = queue;

	printf("%s: [", name);
	
	if (aux){
		print_elem(aux);
		aux = aux->next;

		while (aux != queue) {
			printf(" ");
			print_elem(aux);
			aux = aux->next;
		}
	}

	printf("]\n");
}

int queue_append(queue_t **queue, queue_t *elem){
	queue_t *aux;
	int size;

	if (!queue){
		return -1;
	}

	if (!elem){
		return -2;
	}

	if (!elem_isolated(elem)){
		return -3;
	}

	size = queue_size(*queue);
	switch (size) {
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

int queue_remove(queue_t **queue, queue_t *elem){
	if (!queue){
		return -1;
	}

	if (!elem){
		return -2;
	}

	if (queue_empty(*queue)){
		return -4;
	}

	if (elem_isolated(elem)){
		return -5;
	}

	// Verifica se o elemento pertence a lista
	if (!elem_in_queue(elem, *queue)){
		return -6;
	}

	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;

	if (elem == *queue)
		*queue = elem->next;

	elem->next = NULL;
	elem->prev = NULL;

	if (elem_isolated(*queue))
		*queue = NULL;

	return 0;
}
