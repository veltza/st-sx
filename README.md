# st-sx - simple terminal with sixels

st-sx is a fork of suckless' [st terminal](https://st.suckless.org/) that aims to provide the best sixel support for the st users. It also includes many useful patches such as ligatures and text reflow. And it is the only st fork that supports hyperlinks (OSC 8) and [branch drawing symbols](https://github.com/kovidgoyal/kitty/pull/7681) as well!

## Screenshots

Sixels inside a [tmux](https://github.com/tmux/tmux) session. (apps: [lsix](https://github.com/hackerb9/lsix) and [vv](https://github.com/hackerb9/vv))

![sixels](https://github.com/veltza/st-sx/assets/106755522/0ec5f614-07fc-4843-8455-1a0020e0a0e7)

Branch drawing symbols are supported with built-in glyphs. (app/plugin: [vim-flog](https://github.com/rbong/vim-flog))

![branch-symbols](https://github.com/user-attachments/assets/66c86691-616e-40c7-a4ee-b83848d5d5e6)

## Patches

- Alpha focus highlight
- Anysize simple
- Blinking cursor
- Bold is not bright
- Boxdraw
- Clipboard
- CSI 22 23
- Dynamic cursor color
- Externalpipe
- Font2
- Fullscreen
- Hidecursor
- Hyperlink (OSC 8)
- Keyboard select
- Ligatures
- Nano shortcuts support
- Netwmicon
- Newterm
- Openurlonclick
- Relativeborder
- Scrollback-reflow
- Sixel
- Swapmouse
- Sync
- Undercurl
- Vertcenter
- Visualbell
- Wide glyphs
- Workingdir
- Xresources

## Dependencies

Arch:

```
sudo pacman -S libx11 libxft imlib2 gd
```

FreeBSD:

```
doas pkg install pkgconf imlib2 libgd
```

OpenBSD:

```
doas pkg_add imlib2 gd
```

Ubuntu/Debian:

```
sudo apt install libx11-xcb-dev libxft-dev libimlib2-dev libgd-dev libharfbuzz-dev libpcre2-dev
```

You don't have to install `libharfbuzz-dev`, if you don't use ligatures. Edit config.h and config.mk to disable ligatures.

## Installation

Clone the repo and run `make`:

*Note: If you are building on BSD, you'll need to edit config.mk before running make.*

```
git clone https://github.com/veltza/st-sx
cd st-sx
make
```

Edit `config.h` and add your favorite fonts, colors, etc. and install:

```
sudo make clean install
```

The executable name is `st`.

You can also configure st-sx via Xresources. See xresources-example file.

## Known issues

- Sixels work inside tmux, but...
  * ...sixels might not be enabled if you install it from the repository. In that case, you have to compile tmux yourself with `./configure --enable-sixel`
  * ...some sixels don't show up. The maximum size of sixels in tmux is 1 MB. You can increase the size limit by changing `INPUT_BUF_LIMIT` in `tmux/input.c`. Or after the commit [c26d71d](https://github.com/tmux/tmux/commit/c26d71d3e9425fd5a5f3075888b5425fe6219462), you can change the limit via tmux.conf: `set -g input-buffer-size 1048576`
  * ...sixels may disappear or get stuck. The reason is that the sixel implementation in tmux is not robust yet.

## Thanks

- [suckless.org](https://suckless.org/) and [st](https://st.suckless.org/) contributors
- Bakkeby and his [st-flexipatch](https://github.com/bakkeby/st-flexipatch)
