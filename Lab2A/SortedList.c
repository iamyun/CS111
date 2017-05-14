#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "SortedList.h"
#include <getopt.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	if ((list == NULL) || (list->key != NULL) || (element == NULL) || (element->key) == NULL)
		return;

	SortedList_t *curPrev = list;	//for element right before the one to be inserted
	SortedList_t *cur = list->next;	//for element right after the one to be inserted

	while (cur != list) {	//keep iterating list until found the right position for element
		if (strcmp(element->key, cur->key) < 0)
			break;

		curPrev = cur;
		cur = cur->next;
	}

	if (opt_yield & INSERT_YIELD)
		sched_yield();

	//cirtical section
	element->prev = curPrev;
	element->next = cur;
	curPrev->next = element;
	cur->prev = element;
	//fprintf(stderr, "inserted: %s\n", element->key);
} 

int SortedList_delete(SortedListElement_t *element) {
	if (element == NULL || element->key == NULL)
		return 1;

	//check to make sure that next->prev and prev->next both point at the right node
	if ((element->prev->next != element) || (element->next->prev != element))
		return 1;

	if (opt_yield & DELETE_YIELD)
		sched_yield();

	//critical section
	element->prev->next = element->next;
	element->next->prev = element->prev;
	free(element);
	return 0;

}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	if (list == NULL || list->key != NULL)
		return NULL;

	SortedListElement_t *cur = list->next;

	if (opt_yield & LOOKUP_YIELD)
		sched_yield();

	//critical section
	while (cur != list) {	//iterate through the list until match the key
		if (strcmp(key, cur->key) == 0) {
			//fprintf(stderr, "found: %s\n", cur->key);
			return cur;
		}
		//fprintf(stderr, "%s -> ", cur->key);
		cur = cur->next;
	}

	return NULL;
}

int SortedList_length(SortedList_t *list) {
	if (list == NULL || list->key != NULL)	//check to see if list is corrupted
		return -1;

	int length = 0;
	SortedListElement_t *cur = list->next;

	if (opt_yield & LOOKUP_YIELD)
		sched_yield();

	//critical section
	while (cur != list) {
		if ((cur->prev->next != cur) || (cur->next->prev != cur))	//check if corrupted
			return -1;

		length++;
		cur = cur->next;
	}

	return length;
}