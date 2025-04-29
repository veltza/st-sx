float
clamp(float value, float lower, float upper) {
	return value < lower ? lower : (value > upper ? upper : value);
}

void
changealpha(const Arg *arg)
{
	alpha = clamp(arg->f ?  alpha + arg->f : alpha_def, 0.0, 1.0);
	xloadcols();
	redraw();
}

void
changealphaunfocused(const Arg *arg)
{
	if (alphaUnfocused == -1)
		return;
	alphaUnfocused = clamp(arg->f ?  alphaUnfocused + arg->f : alphaUnfocused_def, 0.0, 1.0);
	xloadcols();
	redraw();
}
