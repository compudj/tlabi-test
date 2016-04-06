/*
 * test-tlabi-cpuid.c
 *
 * Copyright (c) 2015-2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <sys/syscall.h>
#include <error.h>
#include <stddef.h>
#include "thread_local_abi.h"

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define __NR_thread_local_abi       326

static uint64_t nr_loops = 100000;
static int32_t affine_cpu = 0;
static int nr_cpus;

static inline int
thread_local_abi(uint32_t tlabi_nr,
		volatile struct thread_local_abi *tlabi,
		uint32_t feature_mask, int flags)
{
	return syscall(__NR_thread_local_abi, tlabi_nr, tlabi,
			feature_mask, flags);
}

/*
 * __thread_local_abi is recommended as symbol name for the thread-local ABI.
 * Weak attribute is recommended when declaring this variable in libraries.
 */
__thread __attribute__((weak))
volatile struct thread_local_abi __thread_local_abi;

static
int tlabi_cpu_id_register(void)
{
	if (thread_local_abi(0, &__thread_local_abi, TLABI_FEATURE_CPU_ID, 0))
		return -1;
	return 0;
}

static
int32_t read_cpu_id(void)
{
	if (unlikely(!(__thread_local_abi.features & TLABI_FEATURE_CPU_ID)))
		return sched_getcpu();
	return __thread_local_abi.cpu_id;
}

int
do_test_loop(void)
{
	int ret;
	int32_t cpu;
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(affine_cpu, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret) {
		perror("sched_setaffinity");
		return -1;
	}
	if (!__thread_local_abi.features & TLABI_FEATURE_CPU_ID) {
		fprintf(stderr, "[error] Thread-local ABI cpu_id feature is disabled. Unexpected!\n");
		return -1;
	}
	cpu = read_cpu_id();
	if (cpu != affine_cpu) {
		fprintf(stderr, "[error] Current CPU number %d differs from CPU affinity to CPU %d\n",
			cpu, affine_cpu);
		return -1;
	}
	affine_cpu = (affine_cpu + 1) % nr_cpus;
	return 0;
}

int
main(int argc, char **argv)
{
	uint64_t i;

	if (tlabi_cpu_id_register()) {
		fprintf(stderr, "[error] Unable to initialize thread-local ABI cpu_id feature.\n");
		exit(EXIT_FAILURE);
	}
	printf("TLABI features: 0x%x\n", __thread_local_abi.features);

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nr_cpus <= 0) {
		fprintf(stderr, "[error] Unable to get number of configured processors.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < nr_loops; i++) {
		if (do_test_loop())
			exit(EXIT_FAILURE);
	}

	printf("All OK!\n");

	exit(EXIT_SUCCESS);
}
