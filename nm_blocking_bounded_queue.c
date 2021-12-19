/**
 * @file blocking_bounded_queue.c
 * @author Natan Meirov (NatanMeirov@gmail.com)
 * @brief 
 * @version 1.0
 * @date 2021-12-19
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stddef.h> /* size_t, NULL */
#include <stdlib.h> /* malloc, calloc, free */
#include <errno.h>

#include "nm_blocking_bounded_queue.h"


/* ---------------------------------------------- Underlying queue --------------------------------------------- */

/* Defines: */

#define MAX_SIZE_T ((size_t)-1)

struct nm_queue
{
    void** items_;
	size_t capacity_;
	size_t head_;
	size_t tail_;
	size_t items_count_;
};

#define NM_QUEUE_INIT(queue_, init_size_) \
	queue_->capacity_= init_size_; \
	queue_->head_ = 0; \
	queue_->tail_ = 0; \
	queue_->items_count_ = 0;


/* ---------------------------------- nm_queue main API functions implementation ---------------------------------- */

nm_queue* nm_queue_create(size_t init_size_)
{
	nm_queue* queue = NULL;

	if(init_size_ == 0)
	{
		return NULL;
	}

	queue = (nm_queue*)malloc(sizeof(nm_queue));
	if(!queue)
	{
		return NULL;
	}

	queue->items_ = (void**)calloc(init_size_, sizeof(void*)); /* Initialize the whole queue with NULLs */
	if(!queue->items_)
	{
		free(queue);
		return NULL;
	}

    NM_QUEUE_INIT(queue, init_size_);
	return queue;
}


void nm_queue_destroy(nm_queue** queue_, destroy_item_callback callback_)
{
    size_t item_idx;
    size_t items_left;
    size_t queue_capacity;

	if(queue_ && *queue_)
	{
        if(callback_) /* Pointer function is not NULL - destroy action is needed for every item in the nm_queue */
		{
            items_left = (*queue_)->items_count_;
            queue_capacity = (*queue_)->capacity_;
            item_idx = (*queue_)->head_;

            while(items_left-- > 0)
            {
                callback_((*queue_)->items_[item_idx++]);
                item_idx %= queue_capacity;
            }
		}

		free((*queue_)->items_);
		free(*queue_);
        *queue_ = NULL;
	}
}


nm_queue_status nm_queue_enqueue(nm_queue* queue_, void* item_)
{
	if(!queue_ || !item_)
	{
		return NM_QUEUE_UNINITIALIZED_ERROR;
	}

	if(queue_->capacity_ == queue_->items_count_) /* The queue is full */
	{
		return NM_QUEUE_OVERFLOW_ERROR;
	}

	queue_->items_[queue_->tail_++] = item_;
	queue_->tail_ %=  queue_->capacity_;
	++queue_->items_count_;

	return NM_QUEUE_SUCCESS;
}


nm_queue_status nm_queue_dequeue(nm_queue* queue_, void** item_ptr_)
{
	if(!queue_ || !item_ptr_)
	{
		return NM_QUEUE_UNINITIALIZED_ERROR;
	}

	if(queue_->items_count_== 0) /* The queue is empty */
	{
		return NM_QUEUE_UNDERFLOW_ERROR;
	}

	*item_ptr_ = queue_->items_[queue_->head_];
	queue_->items_[queue_->head_++] = NULL;
	queue_->head_ %= queue_->capacity_;
	--queue_->items_count_;

	return NM_QUEUE_SUCCESS;
}


int nm_queue_is_empty(nm_queue* queue_)
{
	if(!queue_)
	{
		return -1;
	}

	return queue_->items_count_ == 0;
}


size_t nm_queue_capacity(nm_queue* queue_)
{
	if(!queue_)
	{
		return MAX_SIZE_T;
	}

	return queue_->capacity_;
}


size_t nm_queue_for_each(nm_queue* queue_, action_callback callback_, void* context_)
{
    size_t item_idx;
    size_t queue_capacity;
    size_t items_left;

    if(!queue_ || !callback_)
    {
        return MAX_SIZE_T;
    }

    items_left = queue_->items_count_;
    queue_capacity = queue_->capacity_;
    item_idx = queue_->head_;

    while(items_left-- > 0)
    {

        if(callback_(queue_->items_[item_idx++], context_) == 0)
        {
            break;
        }
        item_idx %= queue_capacity;
    }

    return queue_->items_count_ - items_left; /* Calculates the total iterated items (1..N) */
}

/* ------------------------------- End of nm_queue main API functions implementation --------------------------- */

/* ------------------------------------------- End of Underlying queue ----------------------------------------- */


/* ----------------------------------------------- Sync utils: ------------------------------------------------- */

#if defined(_WIN32) || defined(_WIN64) || (defined(__CYGWIN__) && !defined(_WIN32))
	static int errno_set(int result)
	{
		if (result != 0)
		{
			errno = result;
			return -1;
		}

		return 0;
	}

	/**
		Create an unnamed semaphore
		@param sem_ The pointer of the semaphore object.
		@param pshared_ The pshared argument indicates whether this semaphore
			is to be shared between the threads (0 or PTHREAD_PROCESS_PRIVATE)
			of a process, or between processes (PTHREAD_PROCESS_SHARED).
		@param value_ The value argument specifies the initial value for
			the semaphore.
		@return If the function succeeds, the return value is 0.
			If the function fails, the return value is -1,
			with errno set to indicate the error.
	*/
	int sem_init(sem_t* sem_, int pshared_, unsigned int value_)
	{
		char buf[24] = {'\0'};
		arch_sem_t* pv;

		if(value_ > (unsigned int)SEM_VALUE_MAX)
		{
			return errno_set(EINVAL);
		}

		if(!(pv = (arch_sem_t*)calloc(1, sizeof(arch_sem_t))))
		{
			return errno_set(ENOMEM);
		}

		if(pshared_ != PTHREAD_PROCESS_PRIVATE)
		{
			sprintf(buf, "Global\\%p", (void*)pv);
		}

		if(!(pv->handle = CreateSemaphoreA(NULL, value_, SEM_VALUE_MAX, buf)))
		{
			free(pv);
			return errno_set(ENOSPC);
		}

		*sem_ = pv;
		return 0;
	}

	/**
		Acquire a semaphore.
		@param sem_ The pointer of the semaphore object.
		@return If the function succeeds, the return value is 0.
			If the function fails, the return value is -1,
			with errno set to indicate the error.
	*/
	int sem_wait(sem_t* sem_)
	{
		arch_sem_t* pv = (arch_sem_t*)sem_;

		if(!pv)
		{
			return errno_set(EINVAL);
		}

		if(WaitForSingleObject(pv->handle, INFINITE) != WAIT_OBJECT_0)
		{
			return errno_set(EINVAL);
		}

		return 0;
	}

	/**
		Release a semaphore.
		@param sem_ The pointer of the semaphore object.
		@return If the function succeeds, the return value is 0.
			If the function fails, the return value is -1,
			with errno set to indicate the error.
	*/
	int sem_post(sem_t* sem_)
	{
		arch_sem_t* pv = (arch_sem_t*)sem_;

		if(!pv)
		{
			return errno_set(EINVAL);
		}

		if(ReleaseSemaphore(pv->handle, 1, NULL) == 0)
		{
			return errno_set(EINVAL);
		}

		return 0;
	}

	/**
		Get the value of a semaphore.
		@param sem_ The pointer of the semaphore object.
		@param value_ptr_ The pointer of the current value of the semaphore.
		@return If the function succeeds, the return value is 0.
			If the function fails, the return value is -1,
			with errno set to indicate the error.
	*/
	int sem_getvalue(sem_t* sem_, int* value_ptr_)
	{
		long previous;
		arch_sem_t* pv = (arch_sem_t*)sem_;

		if(!pv)
		{
			return errno_set(EINVAL);
		}

		switch(WaitForSingleObject(pv->handle, 0))
		{
		case WAIT_OBJECT_0:
			if(!ReleaseSemaphore(pv->handle, 1, &previous))
			{
				return errno_set(EINVAL);
			}

			*value_ptr_ = previous + 1;
			return 0;

		case WAIT_TIMEOUT:
			*value_ptr_ = 0;
			return 0;

		default:
			return errno_set(EINVAL);
		}
	}

	/**
		Destroy a semaphore.
		@param sem_ The pointer of the semaphore object.
		@return If the function succeeds, the return value is 0.
			If the function fails, the return value is -1,
			with errno set to indicate the error.
	*/
	int sem_destroy(sem_t* sem_)
	{
		arch_sem_t* pv = (arch_sem_t*)sem_;

		if(!pv)
		{
			return errno_set(EINVAL);
		}

		if(CloseHandle(pv->handle) == 0)
		{
			return errno_set(EINVAL);
		}

		free(pv);
		*sem_ = NULL;
		return 0;
	}
#elif defined(__linux__)
	#include <semaphore.h>
#else
    #error Environment not supported
#endif


struct mutex_t
{
	sem_t lock_;
};

int mutex_init(mutex_t* mtx_)
{
	return sem_init(&mtx_->lock_, 0, 1);
}

int mutex_destroy(mutex_t* mtx_)
{
	return sem_destroy(&mtx_->lock_);
}

void mutex_lock(mutex_t* mtx_)
{
	sem_wait(&mtx_->lock_);
}

void mutex_unlock(mutex_t* mtx_)
{
	sem_post(&mtx_->lock_);
}


struct barrier_t
{
	unsigned int max_threads_count_;
	unsigned int waiting_threads_count_;
	mutex_t mtx_;
	sem_t turnstile_;
};


int barrier_init(barrier_t* barrier_, int threads_count_)
{
	barrier_->max_threads_count_ = threads_count_;
	barrier_->waiting_threads_count_ = 0;

	if(mutex_init(&barrier_->mtx_) != 0)
	{
		return -1;
	}

	if(sem_init(&barrier_->turnstile_, 0, 0) != 0)
	{
		mutex_destroy(&barrier_->mtx_);
		return -1;
	}

	return 0;
}


int barrier_destroy(barrier_t* barrier_)
{
	return mutex_destroy(&barrier_->mtx_) && sem_destroy(&barrier_->turnstile_);
}


void barrier_wait(barrier_t* barrier_)
{
	unsigned int i, max_threads;
	mutex_lock(&barrier_->mtx_);

	if(++barrier_->waiting_threads_count_ == barrier_->max_threads_count_)
	{
		max_threads = barrier_->max_threads_count_;
		for(i = 0; i < max_threads; ++i)
		{
			sem_post(&barrier_->turnstile_);
		}
	}

	mutex_unlock(&barrier_->mtx_);
	sem_wait(&barrier_->turnstile_);
}

/* --------------------------------------------- End of Sync utils ----------------------------------------------- */

/* Blocking Bounded Queue: */

/* Underlying queue defines: */

typedef nm_queue queue_type;
#define ENQUEUE(queue_, item_) nm_queue_enqueue(queue_, item_)
#define DEQUEUE(queue_, item_ptr_) nm_queue_dequeue(queue_, item_ptr_)
#define SIZE(queue_) nm_queue_capacity(queue_)
#define IS_EMPTY(queue_) nm_queue_is_empty(queue_)

/* Defines: */

struct nm_blocking_bounded_queue
{
    queue_type queue_;
    mutex_t mtx_;
    sem_t free_slots_;
    sem_t occupied_slots_;
    barrier_t enq_waiters_barrier_;
    barrier_t deq_waiters_barrier_;
    atomic_value_t enq_waiters_;
    atomic_value_t deq_waiters_;
    atomic_flag_t is_valid_;
};

/* TODO: implement the BBQ main API function */
