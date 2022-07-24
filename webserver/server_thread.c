#include "request.h"
#include "server_thread.h"
#include "common.h"


struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
};


// pointer to queue (allocated when user types in queue size)
REQ_QUEUE* request_queue;
pthread_t* pthreads; // dynamically allocated depending on how many threads the user wants
pthread_mutex_t queue_mutex;
pthread_cond_t queue_became_nonfull, queue_became_nonempty; 

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

// waits on the queue to be non-empty
static void* helper_thread_do_server_request(void* sv)
{
	const struct server* server = (const struct server*) sv;
	// probably don't need a lock on sv because helper threads read sv only
	while (!server->exiting)
	{
		pthread_mutex_lock(&queue_mutex);
		while (request_queue->curr_size == 0 && !server->exiting)
		{
			pthread_cond_wait(&queue_became_nonempty, &queue_mutex);
		}

		if (server->exiting) {
			pthread_mutex_unlock(&queue_mutex);
			break;
		}

		// now we have the lock, and the queue is nonempty. 
		int connfd;
		assert(pop_front(request_queue, &connfd));  // should succeed because the queue is nonempty
		
		// issue nonfull signal if applicable
		if (request_queue->curr_size == request_queue->max_size - 1){
			pthread_cond_broadcast(&queue_became_nonfull);
		}
		pthread_mutex_unlock(&queue_mutex);

		do_server_request(sv, connfd);
	}
	return NULL;
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	

	/* Lab 4: create queue of max_request size when max_requests > 0 */
	request_queue = create_request_queue(max_requests);

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */
	pthreads = Malloc(nr_threads * sizeof(pthread_t));
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_cond_init(&queue_became_nonempty, NULL);
	pthread_cond_init(&queue_became_nonfull, NULL);
	for (int i = 0; i < nr_threads; i++){
		pthread_create(&pthreads[i], NULL, helper_thread_do_server_request, sv);
	}

	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		pthread_mutex_lock(&queue_mutex);
		while (request_queue->curr_size == request_queue->max_size) {
			pthread_cond_wait(&queue_became_nonfull, &queue_mutex);
		}

		// we now have the lock and can add to queue
		// should succeed because the queue is nonfull
		assert(push_back(request_queue, connfd));

		// issue nonempty signal if applicable
		if (request_queue->curr_size == 1) {
			pthread_cond_broadcast(&queue_became_nonempty);
		}
		pthread_mutex_unlock(&queue_mutex);
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	//First wake all reader threads
	sv->exiting = 1;

	pthread_cond_broadcast(&queue_became_nonempty);
	for (int i = 0; i < sv->nr_threads; i++) {
		pthread_join(pthreads[i], NULL);
	}

	free(pthreads);
	delete_queue(request_queue);

	/* make sure to free any allocated resources */
	free(sv);
}
