static struct {
	Rune code, fold;
} casefolding[] = {
	#include "casefold.inc"
};

Rune
casefold(Rune c)
{
	int hi, lo, mid;

	/* fast ascii case folding */
	if (c < 0x80)
		return c + ((c >= 'A' && c <= 'Z') ? 'a' - 'A' : 0);

	/* binary search in table */
	lo = 0;
	hi = LEN(casefolding) - 1;
	while (lo <= hi) {
		mid = (lo + hi) >> 1;
		if (casefolding[mid].code < c)
			lo = mid + 1;
		else if (casefolding[mid].code > c)
			hi = mid - 1;
		else
			return casefolding[mid].fold;
	}
	return c;
}
