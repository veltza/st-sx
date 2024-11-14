#include <dirent.h>

extern char *argv0;

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
				chdir(get_foreground_cwd());
			} else {
				chdir(getcwd_by_pid(pid));
			}
			setsid();
			execl("/proc/self/exe", argv0, NULL);
			_exit(1);
			break;
		default:
			_exit(0);
		}
	default:
		break;
	}
}

static char*
getcwd_by_pid(pid_t pid) {
	static char cwd[32];

	snprintf(cwd, sizeof cwd, "/proc/%d/cwd", pid);
	return cwd;
}

/* Get the current working directory of the foreground process */
static char *
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
