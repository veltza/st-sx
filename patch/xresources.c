struct {
	char **allocated;
	int count;
	int capacity;
} config_strings;

void
config_replace_string(char **field, const char *new_value)
{
	int i;
	char *old_value = *field;

	if (new_value && old_value && strcmp(new_value, old_value) == 0)
		return;

	if (old_value) {
		for (i = 0; i < config_strings.count; i++) {
			if (config_strings.allocated[i] == old_value) {
				free(old_value);
				if (!new_value) {
					while (++i < config_strings.count)
						config_strings.allocated[i - 1] = config_strings.allocated[i];
					config_strings.count--;
				}
				break;
			}
		}
	} else {
		i = config_strings.count;
	}

	if (!new_value) {
		*field = NULL;
		return;
	}

	if (i == config_strings.count) {
		if (++config_strings.count > config_strings.capacity) {
			config_strings.capacity = MAX(config_strings.capacity * 2, 32);
			config_strings.allocated = realloc(config_strings.allocated,
				config_strings.capacity * sizeof(config_strings.allocated[0]));
		}
	}

	*field = config_strings.allocated[i] = strdup(new_value);
	return;
}

int
config_load_resource(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char **sdst = dst;
	int *idst = dst;
	float *fdst = dst;

	char fullname[256];
	char fullclass[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s",
			opt_name ? opt_name : "st", name);
	snprintf(fullclass, sizeof(fullclass), "%s.%s",
			opt_class ? opt_class : "St", name);
	fullname[sizeof(fullname) - 1] = fullclass[sizeof(fullclass) - 1] = '\0';

	XrmGetResource(db, fullname, fullclass, &type, &ret);
	if (ret.addr == NULL || strncmp("String", type, 64))
		return 1;

	switch (rtype) {
	case STRING:
		config_replace_string(sdst, ret.addr);
		break;
	case INTEGER:
		*idst = strtoul(ret.addr, NULL, 10);
		break;
	case FLOAT:
		*fdst = strtof(ret.addr, NULL);
		break;
	}
	return 0;
}

void
config_init(Display *dpy)
{
	char *resm;
	XrmDatabase db;
	ResourcePref *p;
	int i;

	XrmInitialize();
	resm = XResourceManagerString(dpy);
	if (resm) {
		if ((db = XrmGetStringDatabase(resm)) != NULL) {
			for (i = 0; i < FONT2_XRESOURCES_SIZE; i++)
				config_replace_string(&font2_xresources[i], NULL);
			for (p = resources; p < resources + LEN(resources); p++)
				config_load_resource(db, p->name, p->type, p->dst);
			XrmDestroyDatabase(db);
		}
	}

	LIMIT(cursorstyle, 1, 8);
	xsetcursor(cursorstyle);
	parseurlprotocols();

	/* command line arguments override xresources and config.h */
	cols = (opt_geometry_cols > 0) ? opt_geometry_cols : cols;
	rows = (opt_geometry_rows > 0) ? opt_geometry_rows : rows;
	if (opt_borderperc >= 0) {
		borderperc = opt_borderperc;
		borderpx = 0;
	} else if (opt_borderpx >= 0) {
		borderpx = opt_borderpx;
		borderperc = 0;
	}
	usedfont = (opt_font == NULL) ? font : opt_font;
}

void
config_reload(int sig)
{
	/* Recreate a Display object to have up to date Xresources entries */
	Display *dpy;
	if (!(dpy = XOpenDisplay(NULL)))
		die("Can't open display\n");

	config_init(dpy);
	xloadcols();

	/* nearly like zoomabs() */
	xunloadfonts();
	xloadfonts(usedfont, 0);
	xloadsparefonts();
	cresize(0, 0);
	redraw();
	xhints();

	XCloseDisplay(dpy);
}
