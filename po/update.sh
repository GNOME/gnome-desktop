#!/bin/sh

PACKAGE="gnome-core"

if [ -z $1 ]; then

xgettext --default-domain=$PACKAGE --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $PACKAGE.po \
   || ( rm -f ./$PACKAGE.pot \
&& mv $PACKAGE.po ./$PACKAGE.pot );

else

xgettext --default-domain=gfloppy --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $PACKAGE.po \
   || ( rm -f ./PACKAGE.pot \
&& mv $PACKAGE.po ./$PACKAGE.pot );

mv $1.po $1.po.old && msgmerge $1.po.old $PACKAGE.pot -o $1.po \
&& rm $1.po.old;

fi;
