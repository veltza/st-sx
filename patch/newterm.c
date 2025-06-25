extern char *argv0;
static char *getcwd_by_pid(pid_t);
static char *get_foreground_cwd(void);

void
newterm(const Arg* a)
{
	switch (fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		switch (fork()) {
		case -1:
			fprintf(stderr, "fork failed: %s\n", strerror(errno));
			_exit(1);
			break;
		case 0:
			if (!(a->i & NEWTERM_DISABLE_OSC7) && term.cwd) {
				if (chdir(term.cwd) == 0) {
					/* We need to put the working directory also in PWD, so that the
					 * shell starts in the right directory if cwd is a symlink. */
					setenv("PWD", term.cwd, 1);
				}
			} else if (a->i & NEWTERM_FG_CWD) {
				if (chdir(get_foreground_cwd()) != 0) {
					fprintf(stderr, "newterm failed to change directory to: %s\n", get_foreground_cwd());
				}
			} else {
				if (chdir(getcwd_by_pid(pid)) != 0) {
					fprintf(stderr, "newterm failed to change directory to: %s\n", getcwd_by_pid(pid));
				}
			}
			setsid();
			#if defined(__linux__)
			execl("/proc/self/exe", argv0, NULL);
			#else
			execlp("st", "./st", NULL);
			#endif
			_exit(1);
			break;
		default:
			_exit(0);
		}
	default:
		break;
	}
}

#if defined(__FreeBSD__)
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libprocstat.h>
char *
getcwd_by_pid_and_type(pid_t pid, int type)
{
	static char cwd[PATH_MAX];
	struct procstat *procstat = NULL;
	struct kinfo_proc *procs = NULL;
	struct filestat_list *head;
	struct filestat *fst;
	unsigned int i, count;
	pid_t apid;

	cwd[0] = '\0';
	if (!(procstat = procstat_open_sysctl()))
		goto fail;

	if (!(procs = procstat_getprocs(procstat, type, pid, &count)) || !count)
		goto fail;

	/* We take cwd from the process with the highest PID, because we assume
	 * it is the active process in the group. */
	for (apid = 0, i = 0; i < count; i++) {
		if (apid > procs[i].ki_pid)
			continue;
		if (!(head = procstat_getfiles(procstat, &procs[i], 0)))
			continue;
		STAILQ_FOREACH(fst, head, next) {
			if ((fst->fs_uflags & PS_FST_UFLAG_CDIR) && fst->fs_path) {
				snprintf(cwd, sizeof(cwd), "%s", fst->fs_path);
				apid = procs[i].ki_pid;
				break;
			}
		}
		procstat_freefiles(procstat, head);
	}

fail:
	if (procs)
		procstat_freeprocs(procstat, procs);
	if (procstat)
		procstat_close(procstat);
	return cwd;
}

char *
getcwd_by_pid(pid_t pid)
{
	return getcwd_by_pid_and_type(pid, KERN_PROC_PID);
}

/* Get the current working directory of the foreground process */
char *
get_foreground_cwd(void)
{
	int randompid;
	size_t rlen = sizeof(randompid);
	pid_t pgid = tcgetpgrp(cmdfd);

	/* If we didn't get the fg process group, we return cwd of the shell */
	if (pgid <= 0)
		return getcwd_by_pid(pid);

	/* If PID is randomized, we cannot assume that the process with the
	 * highest PID is the active foreground process in the foreground
	 * process group. In that case, we return cwd of the group leader. */
	if (sysctlbyname("kern.randompid", &randompid, &rlen, NULL, 0) == -1 ||
	    randompid) {
		return getcwd_by_pid(pgid);
	}

	return getcwd_by_pid_and_type(pgid, KERN_PROC_PGRP);
}

#elif defined(__OpenBSD__)
#include <sys/sysctl.h>
char *
getcwd_by_pid(pid_t pid)
{
	static char cwd[PATH_MAX];
	size_t cwdlen = sizeof cwd;
	int name[] = { CTL_KERN, KERN_PROC_CWD, pid };

	if (sysctl(name, 3, cwd, &cwdlen, NULL, 0) == -1)
		cwd[0] = '\0';
	return cwd;
}

/* Get the current working directory of the foreground process group */
char *
get_foreground_cwd(void)
{
	pid_t pgid = tcgetpgrp(cmdfd);

	return getcwd_by_pid(pgid > 0 ? pgid : pid);
}

#else
#include <dirent.h>
char *
getcwd_by_pid(pid_t pid)
{
	static char cwd[32];

	snprintf(cwd, sizeof cwd, "/proc/%d/cwd", pid);
	return cwd;
}

/* Get the current working directory of the foreground process */
char *
get_foreground_cwd(void)
{
	struct dirent *entry;
	char junk;
	DIR *dir;
	pid_t epid, fgpid = 0, pgid = tcgetpgrp(cmdfd);

	/* Find the foreground process in the foreground process group. We assume
	 * that the process with the highest PID is the active foreground process.
	 * The Kitty terminal also uses a similar solution:
	 * https://github.com/kovidgoyal/kitty/blob/70d72b22d89e926e41ba587e4976db53dad21248/kitty/child.py#L444-L457
	 */
	if (pgid > 0 && (dir = opendir("/proc"))) {
		while ((entry = readdir(dir))) {
			if (sscanf(entry->d_name, "%d%c", &epid, &junk) == 1)
				fgpid = (epid > fgpid && getpgid(epid) == pgid) ? epid : fgpid;
		}
		closedir(dir);
	}

	/* Note that if we didn't get the fg process, we fall back to the shell's PID */
	return getcwd_by_pid(fgpid > 0 ? fgpid : pid);
}
#endif
