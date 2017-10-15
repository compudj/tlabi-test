#define _GNU_SOURCE

#define TESTNAME	do_test_linked_lib2

#include "test-template.h"

/* Own state, not shared with other libs. */
static pthread_key_t rseq_key;
static __thread int rseq_registered;

void linked_lib2_fn(void)
{
	do_test_linked_lib2();
}

void linked_lib2_autoreg_fn(void)
{
	fprintf(stderr, "%s\n", __func__);
	if (!rseq_registered) {
		if (rseq_register_current_thread(&__rseq_abi))
			abort();
		/*
		 * Register destroy notifier. Pointer needs to
		 * be non-NULL.
		 */
		if (pthread_setspecific(rseq_key, (void *)0x1))
			abort();
		rseq_registered = 1;
	}
	linked_lib2_fn();
}

static void destroy_rseq_key(void *key)
{
	fprintf(stderr, "%s\n", __func__);
	if (rseq_unregister_current_thread(&__rseq_abi))
		abort();
}

static void __attribute__((constructor)) lib2_init(void)
{
	int ret;

	fprintf(stderr, "%s\n", __func__);
	ret = pthread_key_create(&rseq_key, destroy_rseq_key);
	if (ret) {
		errno = -ret;
		perror("pthread_key_create");
		abort();
	}
}

static void __attribute__((destructor)) lib2_destroy(void)
{
	int ret;

	//FIXME: pthread_key_delete does _NOT_ check if running threads have
	//live keys, and does not call destroy_rseq_key.
	fprintf(stderr, "%s\n", __func__);
	ret = pthread_key_delete(rseq_key);
	if (ret) {
		errno = -ret;
		perror("pthread_key_delete");
		abort();
	}
}
