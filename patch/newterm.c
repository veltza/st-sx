extern char *argv0;
extern const char *env_exe_path;
#if defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
static char *getcwd_by_pid(pid_t);
static char *get_foreground_cwd(void);
#endif
static char *get_current_exe_path(void);
static void exec_current_exe(void);

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
				} else {
					fprintf(stderr, "newterm failed to change directory to: %s\n", term.cwd);
				}
			#if defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
			} else if (a->i & NEWTERM_FG_CWD) {
				if (chdir(get_foreground_cwd()) != 0) {
					fprintf(stderr, "newterm failed to change directory to: %s\n", get_foreground_cwd());
				}
			} else {
				if (chdir(getcwd_by_pid(pid)) != 0) {
					fprintf(stderr, "newterm failed to change directory to: %s\n", getcwd_by_pid(pid));
				}
			#endif
			}
			/* Try to re-execute the current st binary, or st from the PATH */
			setsid();
			exec_current_exe();
			execlp("st", "st", NULL);
			_exit(1);
			break;
		default:
			_exit(0);
		}
	default:
		break;
	}
}

#if defined(__linux__) || defined(__DragonFly__) || defined(__NetBSD__)
#include <dirent.h>
#if defined(__DragonFly__)
#include <sys/sysctl.h>
#endif
char *
getcwd_by_pid(pid_t pid)
{
	#if defined(__DragonFly__)
	static char cwd[PATH_MAX];
	size_t cwdlen = sizeof cwd;
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_CWD, pid };

	if (sysctl(mib, 4, cwd, &cwdlen, NULL, 0) == -1)
		cwd[0] = '\0';
	#else
	static char cwd[32];

	snprintf(cwd, sizeof cwd, "/proc/%d/cwd", pid);
	#endif

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

	#if !defined(__NetBSD__)
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
	#endif

	/* Note that if we didn't get the fg process, we fall back to the shell's PID */
	return getcwd_by_pid((fgpid > 0) ? fgpid : (pgid > 0) ? pgid : pid);
}

char *
get_current_exe_path(void)
{
	#if defined(__DragonFly__)
	const char *symlink = "/proc/curproc/file";
	#elif defined(__NetBSD__)
	const char *symlink = "/proc/curproc/exe";
	#else
	const char *symlink = "/proc/self/exe";
	#endif
	static char exe[PATH_MAX];

	return realpath(symlink, exe) ? exe : NULL;
}

#elif defined(__FreeBSD__)
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

char *
get_current_exe_path(void)
{
	static char exe[PATH_MAX];
	size_t exelen = sizeof exe;
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };

	return sysctl(mib, 4, exe, &exelen, NULL, 0) == 0 ? exe : NULL;
}

#elif defined(__OpenBSD__)
#include <sys/sysctl.h>
char *
getcwd_by_pid(pid_t pid)
{
	static char cwd[PATH_MAX];
	size_t cwdlen = sizeof cwd;
	int mib[] = { CTL_KERN, KERN_PROC_CWD, pid };

	if (sysctl(mib, 3, cwd, &cwdlen, NULL, 0) == -1)
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

char *
get_current_exe_path(void)
{
	/* On OpenBSD, there is no supported or reliable way for a process to
	 * determine the path of its own executable. */
	return NULL;
}

#else
char *
get_current_exe_path(void)
{
	return NULL;
}
#endif

void
exec_current_exe(void)
{
	const char *cur_exe;

	setenv("ST_SX_EXE_PATH", env_exe_path ? env_exe_path : "", 1);

	/* Try to re-execute the current executable */
	if ((cur_exe = get_current_exe_path()) != NULL && cur_exe[0] != '\0')
		execl(cur_exe, argv0, NULL);

	/* If we were unable to re-execute the current executable, the fallback
	 * solution is to launch the exe that was resolved in xsetenv() and
	 * passed via an environment variable. */
	if (env_exe_path && env_exe_path[0] != '\0')
		execlp(env_exe_path, argv0, NULL);
}
