/* These pseudo keys represent the user's keyboard shortcuts */
#define XK_ACTIVATE  -1
#define XK_SEARCHFW  -2
#define XK_SEARCHBW  -3
#define XK_FLASH     -4
#define XK_REGEX     -5
#define XK_URL       -6

void kbds_drawstatusbar(int);
void kbds_pasteintosearch(const char *, int, int);
int kbds_isselectmode(void);
int kbds_issearchmode(void);
int kbds_isflashmode(void);
int kbds_isregexmode(void);
int kbds_isurlmode(void);
int kbds_drawcursor(void);
int kbds_getcursor(int *, int *);
int kbds_keyboardhandler(KeySym, char *, int, int);
