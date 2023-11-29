void keyboard_select(const Arg *dummy) {
    win.mode ^= kbds_keyboardhandler(-1, NULL, 0, 0);
}
