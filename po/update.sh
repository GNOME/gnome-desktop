#!/bin/sh

xgettext --default-domain=gnome-core --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f gnome-core.po \
   || ( rm -f ./gnome-core.pot \
    && mv gnome-core.po ./gnome-core.pot )

msgmerge no.po gnome-core.pot >no-new.po
