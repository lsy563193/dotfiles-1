Dotfiles
========

After cloning this repo, run `install` to automatically set up the development
environment. Note that the install script is idempotent: it can safely be run
multiple times.

For the color scheme to look right, you will also need terminal-specific
support. The configuration for that, along with a whole bunch of other
machine-specific configuration, is located in [dotfiles-local][dotfiles-local].

Dotfiles uses [Dotbot][dotbot] for installation.

Making Local Customizations
---------------------------

You can make local customizations for some programs by editing these files:

* `vim` : `~/.vimrc_local`
* `emacs` : `~/.emacs_local`
* `zsh` : `~/.zshrc_local_before` run before `.zshrc`
* `zsh` : `~/.zshrc_local_after` run after `.zshrc`
* `git` : `~/.gitconfig_local`
* `hg` : `~/.hgrc_local`
* `tmux` : `~/.tmux_local.conf`

License
-------

Copyright (c) 2013-2018 Anish Athalye. Released under the MIT License. See
[LICENSE.md][license] for details.

[dotfiles-local]: https://github.com/anishathalye/dotfiles-local
[dotbot]: https://github.com/anishathalye/dotbot
[license]: LICENSE.md