#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define __NR_ptree 313

struct prinfo {
	long state;
	pid_t pid;
	pid_t parent_pid;
	pid_t first_child_pid;
	pid_t next_sibling_pid;
	long uid;
	char comm[64];
};

static inline int ptree(struct prinfo *processes, unsigned int *count)
{
	return syscall(__NR_ptree, processes, count);
}

static pid_t find_parent(struct prinfo *processes, unsigned int count, pid_t pid)
{
	int i;
	for (i = 0; i < count; ++i) {
		if (processes[i].pid == pid)
			return processes[i].parent_pid;
	}
	return 0;
}

static int hilariously_inefficient_method_of_finding_indentation_level(struct prinfo *processes, unsigned int count, pid_t pid)
{
	int indentation = 0;
	while((pid = find_parent(processes, count, pid)))
		++indentation;
	return indentation;

	/* Bonus points if you can find a reasonable _looking_
	 * algorithm that is even more inefficient. */
}

int main(int argc, char *argv[])
{
	struct prinfo *processes;
	unsigned int count, indentation, i, j;
	pid_t last_ppid;

	indentation = 0;

	count = 32768;
	processes = malloc(sizeof(struct prinfo) * count);
	if (!processes) {
		perror("processes");
		return EXIT_FAILURE;
	}

	memset(processes, 0, sizeof(struct prinfo) * count);

	if (ptree(processes, &count)) {
		perror("ptree");
		return EXIT_FAILURE;
	}

	for (i = 0; i < count; ++i) {
		indentation = hilariously_inefficient_method_of_finding_indentation_level(processes, count, processes[i].pid);
		for (j = 0; j < indentation; ++j)
			putchar('\t');
		printf("%s,%d,%ld,%d,%d,%d,%ld\n", processes[i].comm, processes[i].pid,
			processes[i].state, processes[i].parent_pid, processes[i].first_child_pid,
			processes[i].next_sibling_pid, processes[i].uid);
	}

	printf("%d processes\n", count);

	return EXIT_SUCCESS;
}
