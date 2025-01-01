/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2025 zamfofex */

#ifndef MOONFISH_THREADS
#define MOONFISH_THREADS

#ifdef moonfish_no_threads

#define moonfish_result_t int
#define moonfish_value 0
#define _Atomic

#else

#include <stdatomic.h>

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
