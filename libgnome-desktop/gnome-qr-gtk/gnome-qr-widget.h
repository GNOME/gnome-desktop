/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2026 Canonical Ltd
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <gnome-qr/gnome-qr.h>

G_BEGIN_DECLS

#define GNOME_TYPE_QR_WIDGET (gnome_qr_widget_get_type())
G_DECLARE_FINAL_TYPE (GnomeQrWidget, gnome_qr_widget, GNOME, QR_WIDGET, GtkWidget)

GtkWidget *  gnome_qr_widget_new                  (const char         *text);

void         gnome_qr_widget_set_text             (GnomeQrWidget      *self,
                                                   const char         *text);

const char * gnome_qr_widget_get_text             (GnomeQrWidget      *self);

void         gnome_qr_widget_set_alternative_text (GnomeQrWidget      *self,
                                                   const char         *alternative_text);

const char * gnome_qr_widget_get_alternative_text (GnomeQrWidget      *self);

void         gnome_qr_widget_set_size             (GnomeQrWidget      *self,
                                                   size_t              size);

size_t       gnome_qr_widget_get_size             (GnomeQrWidget      *self);

void         gnome_qr_widget_set_ecc_level        (GnomeQrWidget      *self,
                                                   GnomeQrEccLevel     ecc);

GnomeQrEccLevel gnome_qr_widget_get_ecc_level     (GnomeQrWidget     *self);

G_END_DECLS
