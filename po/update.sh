#!/bin/sh

PACKAGE="gnome-core"

if [ "x$1" = "x" ]; then 

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

echo Now merging $1.po with $PACKAGE.pot, and creating an updated $1.po 

mv $1.po $1.po.old && msgmerge $1.po.old $PACKAGE.pot -o $1.po \
&& rm $1.po.old;

fi;
