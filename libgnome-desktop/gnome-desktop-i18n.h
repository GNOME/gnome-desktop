/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-desktop-i18n.h: I18n stuff for gnome-desktop.

   Copyright (C) 2003 MandrakeSoft

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Frederic Crozat <fcrozat@mandrakesoft.com>
*/

#ifndef EEL_I18N_H
#define EEL_I18N_H

#ifdef ENABLE_NLS
#include <glib.h>

G_CONST_RETURN char *
_gnome_desktop_gettext (const char *str) G_GNUC_FORMAT (1);

#include <libintl.h>
#undef _
#define _(String) _gnome_desktop_gettext(String)

#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain)
#endif


#endif /* EEL_I18N_H */
