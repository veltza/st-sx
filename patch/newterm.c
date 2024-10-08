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
			if (term.cwd) {
				if (chdir(term.cwd) == 0) {
					/* We need to put the working directory also in PWD, so that e.g. bash
					 * starts in the right directory if @directory is a symlink. */
					setenv("PWD", term.cwd, 1);
				}
			} else {
				chdir_by_pid(pid);
			}
			execl("/proc/self/exe", argv0, NULL);
			_exit(1);
			break;
		default:
			_exit(0);
		}
	default:
		wait(NULL);
	}
}

static int
chdir_by_pid(pid_t pid)
{
	char buf[32];
	snprintf(buf, sizeof buf, "/proc/%ld/cwd", (long)pid);
	return chdir(buf);
}
