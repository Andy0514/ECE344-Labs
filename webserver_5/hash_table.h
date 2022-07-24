#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_
#include <stdbool.h>
#include "common.h"
#include "request.h"
#include "queue.h"

#define NUM_BUCKETS 80000


static void
file_data_delete(struct file_data *data)
{
	free(data->file_name);
	free(data);
}

static struct file_data *
file_data_copy(struct file_data* rhs)
{
	struct file_data *data = Malloc(sizeof(struct file_data));

	data->file_name =  Malloc(sizeof(char) * (strlen(rhs->file_name) + 1));
	data->file_buf =  Malloc(sizeof(char) * (strlen(rhs->file_buf) + 1));
	data->file_size = rhs->file_size;

    strcpy(data->file_name, rhs->file_name);
    strcpy(data->file_buf, rhs->file_buf);
	return data;
}

typedef struct ENTRY {
	char* filename;
	struct file_data* data;
	struct ENTRY* next;
} ENTRY;

// Linked list of ENTRY
typedef struct ENTRY_LINKED_LIST {
	ENTRY* head;
} LINKED_LIST;

typedef struct wc {
	/* you can define this struct to have whatever fields you want. */
	LINKED_LIST list[NUM_BUCKETS];
    int max_buffer_size;
    int curr_buffer_size;
} HASH_TABLE;

// the filesize_queue is wholly accessed through this hash_table.h
// No need to put a separate lock on this queue, since this is really 
// a part of the caching system. The lock for the caching system is 
// locally maintained in this file
FILESIZE_QUEUE* filesize_queue;
pthread_mutex_t cache_mutex;


// THE ONLY FUNCTIONS THAT USERS SHOULD CALL ARE: 
// 1. add_to_hash_table
// 2. hash_table_init
// 3. delete_hash_table
// 4. find_in_hash_table
// Of which, 1 and 4 needs a lock. All other functions in this file are called from 
// 1 or 4 (or from single thread), and don't need lock

HASH_TABLE* hash_table_init(int max_buffer_size);
// takes ownership of struct file_data memory
bool add_to_hash_table(HASH_TABLE* wc, char* filename, struct file_data* data);
void delete_from_hash_table(HASH_TABLE* wc, char* filename); 
// deletes the entire hash table
void delete_hash_table(HASH_TABLE* wc);
bool add_to_list(LINKED_LIST* list, char* filename, struct file_data* data);
struct file_data* delete_node_from_list(LINKED_LIST* list, char* filename);
struct file_data* find_in_hash_table(HASH_TABLE* hash_table, char* filename);
void delete_list(LINKED_LIST* list);
void evict_cache(HASH_TABLE* hash_table, int total_size_to_evict);

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

// assumes that this is called in a singlethreaded program with no
// concurrency concerns
HASH_TABLE* hash_table_init(int max_buffer_size) {
    HASH_TABLE* hash_table = Malloc(sizeof(HASH_TABLE));
    hash_table->curr_buffer_size = 0;
    hash_table->max_buffer_size = max_buffer_size;
    	for (int i = 0; i < NUM_BUCKETS; i++) {
		hash_table->list[i].head = NULL;
	}
    filesize_queue = create_filesize_queue();
    pthread_mutex_init(&cache_mutex, NULL);
    return hash_table;
}

// assumes the data's filesize is less than hash table's max size
// return true means that there are no duplicates; return false means duplicates found
bool add_to_hash_table(HASH_TABLE* wc, char* filename, struct file_data* data){
    pthread_mutex_lock(&cache_mutex);
    struct file_data* new_data = file_data_copy(data); // create a new file_data so the old can be deleted
    assert(new_data->file_size <= wc->max_buffer_size);

    int hash_table_remaining_size = wc->max_buffer_size - wc->curr_buffer_size;
    if (new_data->file_size > hash_table_remaining_size){
        // have to evict to get free space
        evict_cache(wc, new_data->file_size - hash_table_remaining_size);
        assert(new_data->file_size <= (wc->max_buffer_size - wc->curr_buffer_size));
    }

	int hash_val = (int)hash_fn(filename);
	assert(hash_val >= 0 && hash_val < NUM_BUCKETS);

	if (add_to_list(&wc->list[hash_val], filename, new_data))
    {
        // add to list successful. Must update current cache size, as well as the list of largest files
        wc->curr_buffer_size += new_data->file_size;
        insert_into_filesize_queue(filesize_queue, new_data->file_size, filename);
        pthread_mutex_unlock(&cache_mutex);
        return true;
    }   

    // insertion failed. delete new_data
    file_data_delete(new_data);
    pthread_mutex_unlock(&cache_mutex);
    return false;
}

// note: we expect filename to exist in hashtable. Also, we expect
// that mutex is locked before calling this
void delete_from_hash_table(HASH_TABLE* wc, char* filename) {
    int hash_val = (int) hash_fn(filename);
	assert(hash_val >= 0 && hash_val < NUM_BUCKETS);

    struct file_data* data = delete_node_from_list(&wc->list[hash_val], filename);
    assert(data != NULL);

    wc->curr_buffer_size -= data->file_size;
    file_data_delete(data);
}


bool add_to_list(LINKED_LIST* list, char* filename, struct file_data* data){
	assert(filename[strlen(filename)] == '\0');

	if (list->head == NULL) {
		list->head = (ENTRY*) malloc(sizeof(ENTRY));
		list->head->filename = (char*) malloc(sizeof(char) * (strlen(filename) + 1));
		strcpy(list->head->filename, filename);
		list->head->data = data;
		list->head->next = NULL;
        return true;
	} else {
		// some filename already existed in the list. see if there are any duplicates
		ENTRY* curr_word_entry = list->head;
		ENTRY* last_word_entry = NULL;
		while (curr_word_entry != NULL) {
			if (strcmp(curr_word_entry->filename, filename) == 0) {

                // duplicate, do not insert
				return false;
			}

			last_word_entry = curr_word_entry;
			curr_word_entry = curr_word_entry->next;
		}

		// reaching here means that no identical entries exist, and we have to add a new entry
		last_word_entry->next = (ENTRY*) malloc(sizeof(ENTRY));
		last_word_entry->next->filename = (char*) malloc(sizeof(char) * (strlen(filename) + 1));
		strcpy(last_word_entry->next->filename, filename);
		last_word_entry->next->data = data;
		last_word_entry->next->next = NULL;
        return true;
	}
}

struct file_data* delete_node_from_list(LINKED_LIST* list, char* filename){
	assert(filename[strlen(filename)] == '\0');

    // some filename already existed in the list. see if there are any duplicates
    ENTRY* curr_word_entry = list->head;
    ENTRY* last_word_entry = NULL;
    while (curr_word_entry != NULL) {
        if (strcmp(curr_word_entry->filename, filename) == 0) {
            // found it!
            struct file_data* data = curr_word_entry->data;

            if (last_word_entry == NULL) {
                list->head = curr_word_entry->next;
            } else {
                last_word_entry->next = curr_word_entry->next;
            }

            free(curr_word_entry->filename);
            free(curr_word_entry);
            return data;
        }

        last_word_entry = curr_word_entry;
        curr_word_entry = curr_word_entry->next;
    }
    return NULL;
}


struct file_data* find_in_hash_table(HASH_TABLE* hash_table, char* filename) {
    
    pthread_mutex_lock(&cache_mutex);
    int hash_val = (int) hash_fn(filename);
    assert(hash_val >= 0 && hash_val < NUM_BUCKETS);

    LINKED_LIST* list = &hash_table->list[hash_val];

    // some filename already existed in the list. see if there are any duplicates
    ENTRY* curr_word_entry = list->head;
    while (curr_word_entry != NULL) {
        if (strcmp(curr_word_entry->filename, filename) == 0) {
            
            // found it!
            // make a copy of the data and return it
            struct file_data* new_data = file_data_copy(curr_word_entry->data);
            pthread_mutex_unlock(&cache_mutex);
            return new_data;
        }

        curr_word_entry = curr_word_entry->next;
    }
    
    pthread_mutex_unlock(&cache_mutex);
    return NULL;
}


void delete_list(LINKED_LIST* list){
	ENTRY* curr_entry = list->head;
	ENTRY* last_entry = NULL;
	while (curr_entry != NULL) {
		free(curr_entry->filename);
        file_data_delete(curr_entry->data);
		last_entry = curr_entry;
		curr_entry = curr_entry->next;
		free(last_entry);
	}
}


void delete_hash_table(HASH_TABLE* wc) {
    for (int i = 0; i < NUM_BUCKETS; i++) {
		delete_list(&(wc->list[i]));
	}
	free(wc);
    delete_queue_fq(filesize_queue);
}

// assumes that this is called with mutex locked
void evict_cache(HASH_TABLE* hash_table, int total_size_to_evict) {
    int evicted_cache = 0;
    while (evicted_cache < total_size_to_evict) {
        char* filename;
        int temp_size;

        // this assert should succeed because if we get here, there is an immediate need for cache 
        // (new file size > current cache size), and the files can be freed up by evicting from cache 
        // (new file size <= max cache size), so pop_front should never fail because filesize_queue
        // should never be more than emptied by this operation
        assert(pop_front_fq(filesize_queue, &filename, &temp_size)); 
        evicted_cache += temp_size;
        delete_from_hash_table(hash_table, filename);
        free(filename);
    }
}

#endif