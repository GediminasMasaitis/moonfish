/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

#ifndef MOONFISH_THREADS
#define MOONFISH_THREADS

#ifdef moonfish_no_threads

#define moonfish_result_t int
#define moonfish_value 0
#define _Atomic

#else

#ifndef moonfish_plan9

#include <stdatomic.h>

#else

#define moonfish_pthreads
#define _Atomic

int cas(int *pointer, int expected, int desired);

static int atomic_compare_exchange_strong(int *pointer, int *expected, int desired)
{
	int value;
	value = *pointer;
	if (value == *expected && cas(pointer, value, desired)) return 1;
	*expected = value;
	return 0;
}

static void atomic_fetch_add(int *pointer, int addend)
{
	int value;
	do value = *pointer;
	while (!cas(pointer, value, value + addend));
}

#endif

#ifndef moonfish_pthreads

#include <threads.h>
#define moonfish_result_t int
#define moonfish_value 0

#else

#include <pthread.h>
#define thrd_t pthread_t
#define thrd_create(thread, fn, arg) pthread_create(thread, NULL, fn, arg)
#define thrd_join pthread_join
#define moonfish_result_t void *
#define moonfish_value NULL
#define thrd_success 0

#endif

#endif

#ifdef moonfish_mini
#undef atomic_compare_exchange_strong
#undef atomic_fetch_add
#endif

#endif
