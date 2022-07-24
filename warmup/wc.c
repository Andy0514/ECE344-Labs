#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define NUM_BUCKETS 200000

typedef struct WORD_ENTRY {
	char* word;
	int occurrence;
	struct WORD_ENTRY* next;
} WORD_ENTRY;

// Linked list of WORD_ENTRY
typedef struct WORD_ENTRY_LINKED_LIST {
	WORD_ENTRY* head;
} LINKED_LIST;

typedef struct wc {
	/* you can define this struct to have whatever fields you want. */
	LINKED_LIST list[NUM_BUCKETS];
} WORD_COUNT;


void add_to_hash_table(WORD_COUNT* wc, char* word);
void add_to_list(LINKED_LIST* list, char* word);
void print_list(LINKED_LIST* list);
void delete_list(LINKED_LIST* list);


static inline unsigned long hash_fn(char* str)
{
	unsigned long h = 332319848ul;
	for (;*str;++str)
	{
		h ^= *str;
		h *= 0x5bd1e995;
		h ^= h >> 15;
	}
	return h % NUM_BUCKETS;
}


WORD_COUNT* wc_init(char *words, long size)
{
	WORD_COUNT* wc;

	wc = (WORD_COUNT*) malloc(sizeof(WORD_COUNT));
	assert(wc);

	for (int i = 0; i < NUM_BUCKETS; i++) {
		wc->list[i].head = NULL;
	}

	int start_index = 0;
	bool last_char_is_space = false;
	for (int i = 0; i < size; i++) {
		if (isspace(words[i]) || (i == size-1)) {

			if (!last_char_is_space){
				// newly reached a space. Form a word from start and end index and add to hash
				last_char_is_space = true;
				int strlen_to_copy = i - start_index; // doesn't include \0
				char* temp_str = (char*) malloc(strlen_to_copy + 1);
				strncpy(temp_str, words + start_index, strlen_to_copy);
				temp_str[strlen_to_copy] = '\0';
				add_to_hash_table(wc, temp_str);
				start_index = i;
				free(temp_str);
			}
			else
			{
				start_index++;
			}
			
		} else {
			// this is not a space
			if (last_char_is_space) {
				start_index++;
				last_char_is_space = false;
			}
		}
	}
	return wc;
}


void wc_output(WORD_COUNT* wc)
{
	for (int i = 0; i < NUM_BUCKETS; i++) {
		print_list(&wc->list[i]);
	}
}

void wc_destroy(WORD_COUNT *wc)
{
	for (int i = 0; i < NUM_BUCKETS; i++) {
		delete_list(&wc->list[i]);
	}
	free(wc);
}

void add_to_hash_table(WORD_COUNT* wc, char* word) {
	int hash_val = (int)hash_fn(word);
	assert(hash_val >= 0 && hash_val < NUM_BUCKETS);

	add_to_list(&wc->list[hash_val], word);
}


// Assumes that word is null terminated
void add_to_list(LINKED_LIST* list, char* word){
	assert(word[strlen(word)] == '\0');

	if (list->head == NULL) {
		list->head = (WORD_ENTRY*) malloc(sizeof(WORD_ENTRY));
		list->head->word = (char*) malloc(sizeof(char) * (strlen(word) + 1));
		strcpy(list->head->word, word);
		list->head->occurrence = 1;
		list->head->next = NULL;
	} else {
		// some words already existed in the list. Search existing entry for occurrence
		WORD_ENTRY* curr_word_entry = list->head;
		WORD_ENTRY* last_word_entry = NULL;
		while (curr_word_entry != NULL) {
			if (strcmp(curr_word_entry->word, word) == 0) {
				curr_word_entry->occurrence++;
				return;
			}

			last_word_entry = curr_word_entry;
			curr_word_entry = curr_word_entry->next;
		}


		// reaching here means that no identical words exist, and we have to add a new entry
		last_word_entry->next = (WORD_ENTRY*) malloc(sizeof(WORD_ENTRY));
		last_word_entry->next->word = (char*) malloc(sizeof(char) * (strlen(word) + 1));
		strcpy(last_word_entry->next->word, word);
		last_word_entry->next->occurrence++;
		last_word_entry->next->next = NULL;
	}
}


void print_list(LINKED_LIST* list) {

	WORD_ENTRY* curr_entry = list->head;
	while (curr_entry != NULL) {
		printf("%s:%d\n", curr_entry->word, curr_entry->occurrence);
		curr_entry = curr_entry->next;
	}
}


void delete_list(LINKED_LIST* list){
	WORD_ENTRY* curr_entry = list->head;
	WORD_ENTRY* last_entry = NULL;
	while (curr_entry != NULL) {
		free(curr_entry->word);
		last_entry = curr_entry;
		curr_entry = curr_entry->next;
		free(last_entry);
	}
}

