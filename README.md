# st-sx - simple terminal with sixels

st-sx is a fork of suckless' [st terminal](https://st.suckless.org/) that aims to provide the best sixel support for st users. It also includes many useful patches such as ligatures and text reflow.

## Screenshot

![1](https://github.com/veltza/st-sx/assets/106755522/0ec5f614-07fc-4843-8455-1a0020e0a0e7)

Sixels inside a [tmux](https://github.com/tmux/tmux) session (apps: [lsix](https://github.com/hackerb9/lsix) and [vv](https://github.com/hackerb9/vv))

## Patches

- Alpha focus highlight
- Anysize simple
- Blinking cursor
- Bold is not bright
- Boxdraw
- Clipboard
- CSI 22 23
- Dynamic cursor color
- Font2
- Hidecursor
- Keyboard select
- Ligatures
- Netwmicon
- Newterm
- Openurlonclick
- Scrollback-reflow
- Sixel
- Swapmouse
- Sync
- Undercurl
- Vertcenter
- Wide glyphs
- Workingdir
- Xresources

## Dependencies

Arch:

```
sudo pacman -S libx11 libxft imlib2 gd
```

Ubuntu:

```
sudo apt install libx11-xcb-dev libxft-dev libimlib2-dev libgd-dev libharfbuzz-dev
```

You don't have to install `libharfbuzz-dev`, if you don't use ligatures. Edit config.h and config.mk to disable ligatures.

## Installation

Clone the repo and run `make`:

```
git clone https://github.com/veltza/st-sx
cd st-sx
make
```

Edit `config.h` and add your favorite fonts, colors etc. and install:

```
sudo make install
```

The executable name is `st`.

You can also configure st-sx via Xresources. See xresources-example file.

## Known issues

- Sixels work inside tmux, but...
  * ...sixels are not enabled in the release version of tmux. You have to compile it yourself with `./configure --enable-sixel`.
  * ...some sixels don't show up. The maximum size of sixels in tmux is 1 MB. You can increase the size limit by changing `INPUT_BUF_LIMIT` in `tmux/input.c`.
  * ...sixels may disappear or get stuck. The reason is that the sixel implementation in tmux is not robust yet.

## Thanks

- [suckless.org](https://suckless.org/) and [st](https://st.suckless.org/) contributors
- Bakkeby's [st-flexipatch](https://github.com/bakkeby/st-flexipatch)
