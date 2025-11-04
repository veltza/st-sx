int
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
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
		*sdst = ret.addr;
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

	memset(font2_xresources, 0, sizeof(font2_xresources));

	XrmInitialize();
	resm = XResourceManagerString(dpy);
	if (resm) {
		db = XrmGetStringDatabase(resm);
		for (p = resources; p < resources + LEN(resources); p++)
			resource_load(db, p->name, p->type, p->dst);
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
reload_config(int sig)
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
