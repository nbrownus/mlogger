### Relase instructions

You will need [ronn](https://github.com/rtomayko/ronn) which is a utility that converts markdown like text to roff.

    gem install ronn

Update the changelog using `dch` or something similar

Run `./package.sh` which will generate many things under ./release and well as the man page

If the deb is stable, copy it into dist and commit