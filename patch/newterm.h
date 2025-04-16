enum newterm_options {
	NEWTERM_SHELL_CWD    = 0,      /* open terminal in cwd of shell */
	NEWTERM_FG_CWD       = 1 << 0, /* open terminal in cwd of foreground process */
	NEWTERM_DISABLE_OSC7 = 1 << 1, /* disable OSC 7 support */
};

void newterm(const Arg *);
