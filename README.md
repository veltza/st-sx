# st - simple terminal

This is my fork of [st terminal](https://st.suckless.org/) with several useful patches like ligatures, sixels and text reflow. I've also made some fixes here and there to improve some patches.

## Patches

- Alpha focus highlight
- Anysize simple
- Bold is not bright
- Boxdraw
- Clipboard
- CSI 22 23
- Dynamic cursor color
- Font2
- Hidecursor
- Ligatures
- Netwmicon
- Newterm
- Openurlonclick
- Scrollback-reflow
- Sixel
- Swapmouse
- Undercurl
- Wide glyphs
- Workingdir

## Installation

Read README file.

## Known issues

- Sixels may overlap text when text is reflowed.
- Sixels work inside tmux, but may sometimes disappear. The reason is that the sixel implementation in tmux is not robust yet.

## Thanks

- [suckless.org](https://suckless.org/) and [st](https://st.suckless.org/) contributors
- Bakkeby's [st-flexipatch](https://github.com/bakkeby/st-flexipatch)
