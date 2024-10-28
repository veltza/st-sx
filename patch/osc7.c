int
hex2int(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return -1;
}

int
urldecode(const char *url, char *decoded, int decodedsize)
{
	int i, h1, h2, decodedlen;

	for (decodedlen = 0, i = 0; url[i]; i++) {
		if (url[i] == '%' &&
		    (h1 = hex2int(url[i+1])) >= 0 && (h2 = hex2int(url[i+2])) >= 0) {
			decoded[decodedlen++] = (h1 << 4) | h2;
			i += 2;
		} else {
			decoded[decodedlen++] = url[i];
		}
		if (decodedlen == decodedsize) {
			decoded[decodedlen-1] = '\0';
			return decodedlen;
		}
	}
	decoded[decodedlen] = '\0';
	return decodedlen;
}

int
osc7parsecwd(const char *uri)
{
	const char *auth, *host, *hostend;
	char *path, decoded[PATH_MAX], thishost[_POSIX_HOST_NAME_MAX];
	int decodedlen, hostlen;

	if (!term.cwd) {
		term.cwd = xmalloc(sizeof(decoded));
		term.cwd[0] = '\0';
	}

	/* reset cwd if uri is empty */
	if (strlen(uri) == 0) {
		term.cwd[0] = '\0';
		return 1;
	}

	/* decode uri */
	decodedlen = urldecode(uri, decoded, sizeof(decoded));
	if (decodedlen == sizeof(decoded)) {
		fprintf(stderr, "erresc (OSC 7): uri is too long\n");
		return 0;
	}

	/* check scheme */
	if (decodedlen < 5 || strncmp("file:", decoded, 5) != 0) {
		fprintf(stderr, "erresc (OSC 7): scheme is not supported: '%s'\n", uri);
		return 0;
	}

	/* find start of authority */
	if (decodedlen < 7 || decoded[5] != '/' || decoded[6] != '/') {
		fprintf(stderr, "erresc (OSC 7): invalid uri: '%s'\n", uri);
		return 0;
	}
	auth = decoded + 7;

	/* find start of path and reset cwd if path is missing */
	if ((path = strchr(auth, '/')) == NULL) {
		term.cwd[0] = '\0';
		return 1;
	}

	/* ignore path if host is not localhost */
	host = ((host = memchr(auth, '@', path - auth)) != NULL) ? host+1 : auth;
	hostend = ((hostend = memchr(host, ':', path - host)) != NULL) ? hostend : path;
	hostlen = hostend - host;
	if (gethostname(thishost, sizeof(thishost)) < 0)
		thishost[0] = '\0';
	if (hostlen > 0 &&
	    !(hostlen == 9 && strncmp("localhost", host, hostlen) == 0) &&
	    !(hostlen == strlen(thishost) && strncmp(host, thishost, hostlen) == 0)) {
		return 0;
	}

	memcpy(term.cwd, path, decodedlen - (path - decoded) + 1);
	return 1;
}
