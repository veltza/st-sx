void
extpipe(const Arg *arg, int in)
{
	int to[2];
	char buf[UTF_SIZ];
	void (*oldsigpipe)(int);
	int x, y, y1, y2, len, newline;
	Line line;

	if (pipe(to) == -1)
		return;

	switch (fork()) {
	case -1:
		close(to[0]);
		close(to[1]);
		return;
	case 0:
		dup2(to[0], STDIN_FILENO);
		close(to[0]);
		close(to[1]);
		if (in)
			dup2(csdfd, STDOUT_FILENO);
		close(csdfd);
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "st: execvp %s\n", ((char **)arg->v)[0]);
		perror("failed");
		exit(0);
	}

	close(to[0]);

	/* ignore sigpipe for now, in case child exists early */
	oldsigpipe = signal(SIGPIPE, SIG_IGN);

	y1 = IS_SET(MODE_ALTSCREEN) ? 0 : -term.histf;
	for (y2 = term.row-1; y2 >= 0 && tlinelen(term.line[y2]) == 0; y2--)
		;
	newline = 0;

	for (y = y1; y <= y2; y++) {
		line = TLINEABS(y);
		len = tlinelen(line);
		for (x = 0; x < len; x++) {
			if (xwrite(to[1], buf, utf8encode(line[x].u, buf)) < 0)
				break;
		}
		if ((newline = len > 0 && (line[len-1].mode & ATTR_WRAP)))
			continue;
		if (xwrite(to[1], "\n", 1) < 0)
			break;
		newline = 0;
	}

	if (newline)
		(void)xwrite(to[1], "\n", 1);
	close(to[1]);

	/* restore old sigpipe handler */
	signal(SIGPIPE, oldsigpipe);
}

void
externalpipe(const Arg *arg) {
	extpipe(arg, 0);
}

void
externalpipein(const Arg *arg) {
	extpipe(arg, 1);
}
