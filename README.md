# st - simple terminal

This is my fork of [st terminal](https://st.suckless.org/) with several useful patches that are patched using [st-flexipatch](https://github.com/bakkeby/st-flexipatch) and [flexipatch-finalizer](https://github.com/bakkeby/flexipatch-finalizer). I've also made some fixes here and there to improve some patches.

## Patches

- Alpha
- Alpha focus highlight
- Anysize simple
- Bold is not bright
- Boxdraw
- Clipboard
- CSI 23 23
- Dynamic cursor color
- Font2
- Hidecursor
- Ligatures
- Netwmicon
- Newterm
- Openurlonclick
- Scrollback-reflow (*)
- Sixel
- Swapmouse
- Wide glyphs
- Workingdir

(*) The [scrollback-reflow](https://st.suckless.org/patches/scrollback/) patch is not available on the main branch of st-flexipatch, so it has been patched manually.

## Installation

Read README file.

## Known issues

- Sixels and text reflow don't work well together. The images may overlap the text.
- Sixels don't work in tmux. Use [WezTerm](https://github.com/wez/wezterm).

## Thanks

- [suckless.org](https://suckless.org/) and [st](https://st.suckless.org/) contributors
- Bakkeby's [st-flexipatch](https://github.com/bakkeby/st-flexipatch)
