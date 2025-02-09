void keyboard_select(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
}

void searchforward(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
	kbds_keyboardhandler(XK_SEARCHFW, NULL, 0, 0);
}

void searchbackward(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
	kbds_keyboardhandler(XK_SEARCHBW, NULL, 0, 0);
}

void keyboard_flash(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
	kbds_keyboardhandler(XK_FLASH, NULL, 0, 0);
}

void keyboard_regex(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
	kbds_keyboardhandler(XK_REGEX, NULL, 0, 0);
}

void keyboard_url(const Arg *dummy)
{
	win.mode ^= kbds_keyboardhandler(XK_ACTIVATE, NULL, 0, 0);
	kbds_keyboardhandler(XK_URL, NULL, 0, 0);
}
