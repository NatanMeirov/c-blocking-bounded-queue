/**
 * @file blocking_bounded_queue.h
 * @author Natan Meirov (NatanMeirov@gmail.com)
 * @brief 
 * @version 1.0
 * @date 2021-12-19
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef NM_BLOCKING_BOUNDED_QUEUE_H
#define NM_BLOCKING_BOUNDED_QUEUE_H


/* Includes: */
#include <stddef.h> /* size_t, NULL */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* ---------------------------------------------- Underlying queue --------------------------------------------- */

/* Defines: */
typedef enum nm_queue_status
{
    NM_QUEUE_SUCCESS,
    NM_QUEUE_UNINITIALIZED_ERROR,
    NM_QUEUE_OVERFLOW_ERROR,
    NM_QUEUE_UNDERFLOW_ERROR
} nm_queue_status;

typedef struct nm_queue nm_queue;

/**
 * @brief An action callback function that will be called on each element of the queue when destroying the queue
 * @param[in] element_: A pointer to an element to destroy
 * @return None
 */
typedef void (*destroy_item_callback)(void* element_);

/**
 * @brief An action callback function that will be called for each element
 * @param[in] element_: A pointer to an element
 * @param[in] context_: A pointer to the context to pass to the action callback function, when the function is called
 * @return int - 0, if the iteration should stop on the current element / 1, if the iteration should continue to the next element
 */
typedef int (*action_callback)(const void* element_, void* context_);


/* --------------------------------------- End of nm_queue main API functions ---------------------------------- */

/**
 * @brief Dynamically creates a new nm_queue object of a given size
 * @param[in] init_size_: The size of the queue to create
 * @return nm_queue* - on success / NULL - on failure
 *
 * @warning The queue has fixed size, and does not have the ability to grow and shrink on demand
 * @warning If init_size_ is 0: function will fail and return NULL
 */
nm_queue* nm_queue_create(size_t init_size_);


/**
 * @brief Dynamically deallocates a previously allocated nm_queue, NULLs the nm_queue's pointer
 * @param[in] queue_: A nm_queue to deallocate
 * @param[in] callback_: A function pointer to be used to destroy each element in the given nm_queue,
 * 			 				   or a NULL if no such destroy is required
 * @return None
 *
 */
void nm_queue_destroy(nm_queue** queue_, destroy_item_callback callback_);


/**
 * @brief Inserts an item to the end of the queue, if the queue is not full
 * @param[in] queue_: A nm_queue to insert an item to
 * @param[in] item_: The item to insert to the end of the queue, cannot be NULL
 * @return nm_queue_status - success or error status code
 * @retval NM_QUEUE_SUCCESS on success
 * @retval NM_QUEUE_UNINITIALIZED_ERROR on error - a given pointer is NULL
 * @retval NM_QUEUE_OVERFLOW_ERROR on error - reached size limit, no more room to add another item
 *
 * @warning If item_ is NULL: function will fail and return NM_QUEUE_UNINITIALIZED_ERROR
 */
nm_queue_status nm_queue_enqueue(nm_queue* queue_, void* item_);


/**
 * @brief Removes an item from the beginning of the queue, if the queue is not empty
 * @param[in] queue_: A queue to remove an item from
 * @param[out] item_ptr_: A pointer to a variable that used to return the wanted item value by reference
 * @return nm_queue_status - success or error status code
 * @retval NM_QUEUE_SUCCESS on success
 * @retval NM_QUEUE_UNINITIALIZED_ERROR on error - a given pointer is NULL
 * @retval NM_QUEUE_UNDERFLOW_ERROR on error - queue is empty, no more items to remove
 *
 * @warning If item_ptr_ is NULL: function will fail and return NM_QUEUE_UNINITIALIZED_ERROR
 */
nm_queue_status nm_queue_dequeue(nm_queue* queue_, void** item_ptr_);


/**
 * @brief Checks if a given nm_queue is empty or not
 * @param[in] queue_: A queue to check if is empty
 * @return int - 0 if queue is not empty or 1 if queue is empty, on success / -1, on failure
 */
int nm_queue_is_empty(nm_queue* queue_);


/**
 * @brief Returns the given nm_queue's capacity
 * @param[in] queue_: A queue to check its capacity
 * @return size_t - queue's capacity, on success
            / MAX_SIZE_T (-1 as size_t - is the maximum size_t value), on failure
 */
size_t nm_queue_capacity(nm_queue* queue_);


/**
 * @brief Iterates over all the elements in the queue, and triggers an action callback function on every single element
 * @details The user provides an action callback function that will be called for each element,
 *          if the action callback function returns 0 for an element - the iteration will stop,
 *			if the action callback function returns 1 for an element - the iteration will continue
 * @param[in] queue_: A queue to iterate over
 * @param[in] callback_: User provided action callback function pointer to be invoked for each element
 * @param[in] context_: User provided context, that will be sent to action callback function
 * @return size_t - number of times the user function was invoked (if successfully iterated all the queue items [ == number of items in the queue]), on success
			/ MAX_SIZE_T (-1 as size_t - is the maximum size_t value), on failure
 */
size_t nm_queue_for_each(nm_queue* queue_, action_callback callback_, void* context_);

/* --------------------------------------- End of nm_queue main API functions ---------------------------------- */
/* ------------------------------------------- End of Underlying queue ----------------------------------------- */

/* ----------------------------------------------- Sync utils: ------------------------------------------------- */

/* Atomics: */
typedef size_t atomic_value_t;
#define ATOMIC_VALUE_SET(atomic_val_, new_val_) (*atomic_val_) = (new_val_)
#define ATOMIC_VALUE_SET_IF(atomic_val_, cond_val_, new_val_) if((*atomic_val_) == (cond_val_)) { (*atomic_val_) = (new_val_); }
#define ATOMIC_VALUE_ADD(atomic_val_, val_to_add_) (*atomic_val_) += (val_to_add)
#define ATOMIC_VALUE_SUB(atomic_val_, val_to_add_) (*atomic_val_) -= (val_to_add)
typedef unsigned char atomic_flag_t;
#define ATOMIC_FLAG_SET(atomic_flag_, new_val_) (*atomic_flag_) = (new_val_)
#define ATOMIC_FLAG_SET_IF(atomic_flag_, cond_val_, new_val_) if((*atomic_flag_) == (cond_val_)) { (*atomic_flag_) = (new_val_); }
#define ATOMIC_FLAG_LOAD(atomic_flag_) (*atomic_flag_)

#if defined(_WIN32) || defined(_WIN64) || (defined(__CYGWIN__) && !defined(_WIN32))
    #include <winsock.h>

    #ifndef PTHREAD_PROCESS_SHARED
    #define PTHREAD_PROCESS_PRIVATE	0
    #define PTHREAD_PROCESS_SHARED	1
    #endif

    /* Support POSIX.1b semaphores.  */
    #ifndef _POSIX_SEMAPHORES
    #define _POSIX_SEMAPHORES 200809L
    #endif

    #ifndef SEM_VALUE_MAX
    #define SEM_VALUE_MAX 2147483647
    #endif

    #include <stdio.h> /* sprintf */

    typedef void* sem_t;
    typedef struct arch_sem_t
    {
        HANDLE handle;
    } arch_sem_t;

    int sem_init(sem_t* sem_, int pshared_, unsigned int value_);
    int sem_wait(sem_t* sem_);
    int sem_post(sem_t* sem_);
    int sem_getvalue(sem_t* sem_, int* value_);
    int sem_destroy(sem_t* sem_);
#elif defined(__linux__)
	#include <semaphore.h>
#else
    #error Environment not supported
#endif

typedef struct mutex_t mutex_t;

int mutex_init(mutex_t* mtx_);
int mutex_destroy(mutex_t* mtx_);
void mutex_lock(mutex_t* mtx_);
void mutex_unlock(mutex_t* mtx_);

typedef struct barrier_t barrier_t;

int barrier_init(barrier_t* barrier_, int threads_count_);
int barrier_destroy(barrier_t* barrier_);
void barrier_wait(barrier_t* barrier_);

/* --------------------------------------------- End of Sync utils ----------------------------------------------- */


/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------ Blocking Bounded Queue: -------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

/* Defines: */

typedef struct nm_blocking_bounded_queue nm_blocking_bounded_queue;

typedef enum nm_bbq_status
{
    NM_BBQ_SUCCESS,
    NM_BBQ_UNINITIALIZED_ERROR,
    NM_BBQ_IS_CLOSED
} nm_bbq_status;

typedef void (*bbq_destruction_policy_callback)(void* element_, void* callback_context_);


/* TODO: Add documentation for each API function and about the destruction_policy */
nm_blocking_bounded_queue* nm_blocking_bounded_queue_create(size_t init_capacity_);
void nm_blocking_bounded_queue_destroy(nm_blocking_bounded_queue** bbq_, bbq_destruction_policy_callback callback_, void* callback_context_);
nm_bbq_status nm_blocking_bounded_queue_put(nm_blocking_bounded_queue* bbq_, void* item_);
nm_bbq_status nm_blocking_bounded_queue_take(nm_blocking_bounded_queue* bbq_, void** item_ptr_);
size_t nm_blocking_bounded_queue_size(nm_blocking_bounded_queue* bbq_);
int nm_blocking_bounded_queue_is_empty(nm_blocking_bounded_queue* bbq_);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NM_BLOCKING_BOUNDED_QUEUE_H */
