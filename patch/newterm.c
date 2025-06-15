#include <dirent.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#endif

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
			#ifdef __OpenBSD__
			execlp("st", "./st", NULL);
			#else
			execl("/proc/self/exe", argv0, NULL);
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

#ifdef __OpenBSD__
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
