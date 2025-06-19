#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <spdk/env.h>
#include <spdk/event.h>
#include <spdk/log.h>
#include <spdk/thread.h>

enum run_mode {
	MODE_ALLOC_AND_FREE,
	MODE_ALLOC_AND_WAIT,
	MODE_RECLAIM,
};

static enum run_mode g_run_mode = MODE_ALLOC_AND_FREE;
static const char *g_shm_name = "memory_benchmark";

static double
get_time_diff(const struct timespec *start, const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void
spdk_benchmark_run(void *arg1)
{
	struct timespec *start = arg1;
	struct timespec end;
	double time_diff;

	clock_gettime(CLOCK_MONOTONIC, &end);
	time_diff = get_time_diff(start, &end);

	switch (g_run_mode) {
	case MODE_ALLOC_AND_FREE:
	case MODE_ALLOC_AND_WAIT:
		printf("Time to initialize SPDK application with 6GB memory: %f seconds\n", time_diff);
		break;
	case MODE_RECLAIM:
		printf("Time to reclaim SPDK application memory: %f seconds\n", time_diff);
		break;
	}

	free(start);

	if (g_run_mode == MODE_ALLOC_AND_WAIT) {
		printf("SPDK application initialized. Waiting to be killed...\n");
		/* Do nothing, wait for kill */
	} else {
		spdk_app_stop(0);
	}
}
int
main(int argc, char **argv)
{
	struct spdk_app_opts opts = {};
	int rc;
	struct timespec *start;
	bool use_shm;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <alloc_free|alloc_wait|reclaim> [use_shm]\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "alloc_free") == 0) {
		g_run_mode = MODE_ALLOC_AND_FREE;
	} else if (strcmp(argv[1], "alloc_wait") == 0) {
		g_run_mode = MODE_ALLOC_AND_WAIT;
	} else if (strcmp(argv[1], "reclaim") == 0) {
		g_run_mode = MODE_RECLAIM;
	} else {
		fprintf(stderr, "Invalid mode: %s\n", argv[1]);
		return 1;
	}

	use_shm = (argc > 2 && strcmp(argv[2], "use_shm") == 0);

	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = g_shm_name;
	opts.mem_size = 6 * 1024;

	if (use_shm) {
		/*
		 * Using a specific shm_id tells SPDK to create a file in hugetlbfs.
		 * This file backs the memory, ensuring it persists even if the
		 * process is killed. This allows for a true memory reclaim.
		 */
		opts.shm_id = 1;
	} else if (g_run_mode == MODE_RECLAIM) {
		/*
		 * In the non-persistent case, we still tell the reclaim process
		 * to try to attach to existing memory first. This will fail
		 * after a kill since anonymous memory is freed by the kernel,
		 * forcing a fresh allocation. This demonstrates the "fake" reclaim.
		 */
		opts.shm_id = -1;
	}

	start = malloc(sizeof(struct timespec));
	if (start == NULL) {
		SPDK_ERRLOG("Failed to allocate memory for start time\n");
		return -1;
	}

	clock_gettime(CLOCK_MONOTONIC, start);

	rc = spdk_app_start(&opts, spdk_benchmark_run, start);
	if (rc) {
		SPDK_ERRLOG("ERROR starting application\n");
		free(start);
		return rc;
	}

	if (g_run_mode == MODE_ALLOC_AND_FREE) {
		struct timespec start_free, end_free;

		clock_gettime(CLOCK_MONOTONIC, &start_free);
		spdk_app_fini();
		clock_gettime(CLOCK_MONOTONIC, &end_free);

		printf("Time to free SPDK application memory: %f seconds\n",
		       get_time_diff(&start_free, &end_free));

	} else if (g_run_mode == MODE_RECLAIM) {
		spdk_app_fini();
	}

	return rc;
}
