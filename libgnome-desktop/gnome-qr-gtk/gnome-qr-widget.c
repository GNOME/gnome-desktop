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

#include "config.h"

#include "gnome-qr-widget.h"

#include <glib/gi18n-lib.h>
#include <gnome-qr/gnome-qr.h>
#include <gdk/gdk.h>

/**
 * GnomeQrWidget:
 *
 * The `GnomeQrWidget` widget displays a rendered QR Code.
 *
 * ## CSS nodes
 *
 * `GnomeQrWidget` has a single CSS node with the name `gnome-qr-code`.
 * It can be themed using the `color` and `background-color` properties.
 *
 * ## Accessibility
 *
 * `GnomeQrWidget` uses the `GTK_ACCESSIBLE_ROLE_IMG` role.
 */

struct _GnomeQrWidget
{
  GtkWidget parent_instance;

  char *text;
  char *alternative_text;
  size_t size;
  GnomeQrEccLevel ecc_level;
  GdkRGBA fg_color;

  GdkTexture *texture;
  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (GnomeQrWidget, gnome_qr_widget, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_TEXT,
  PROP_ALTERNATIVE_TEXT,
  PROP_SIZE,
  PROP_ECC_LEVEL,
  N_PROPS
};

enum {
  SIGNAL_CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[N_SIGNALS] = { 0 };

static void gnome_qr_widget_update (GnomeQrWidget *self);

static void
gnome_qr_widget_dispose (GObject *object)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->texture);

  G_OBJECT_CLASS (gnome_qr_widget_parent_class)->dispose (object);
}

static void
gnome_qr_widget_finalize (GObject *object)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (object);

  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->alternative_text, g_free);

  G_OBJECT_CLASS (gnome_qr_widget_parent_class)->finalize (object);
}

static void
gnome_qr_widget_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (object);

  switch (prop_id) {
  case PROP_TEXT:
    g_value_set_string (value, self->text);
    break;

  case PROP_ALTERNATIVE_TEXT:
    g_value_set_string (value, self->alternative_text);
    break;

  case PROP_SIZE:
    g_value_set_uint64 (value, self->size);
    break;

  case PROP_ECC_LEVEL:
    g_value_set_enum (value, self->ecc_level);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gnome_qr_widget_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (object);

  switch (prop_id) {
  case PROP_TEXT:
    gnome_qr_widget_set_text (self, g_value_get_string (value));
    break;

  case PROP_ALTERNATIVE_TEXT:
    gnome_qr_widget_set_alternative_text (self, g_value_get_string (value));
    break;

  case PROP_SIZE:
    gnome_qr_widget_set_size (self, g_value_get_uint64 (value));
    break;

  case PROP_ECC_LEVEL:
    gnome_qr_widget_set_ecc_level (self, g_value_get_enum (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gnome_qr_widget_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (widget);
  int width, height, size;
  int x, y;

  if (!self->texture)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  size = MIN (width, height);
  x = (width - size) / 2;
  y = (height - size) / 2;

  gtk_snapshot_append_scaled_texture (snapshot,
                                      self->texture,
                                      GSK_SCALING_FILTER_NEAREST,
                                      &GRAPHENE_RECT_INIT (x, y, size, size));
}

static void
gnome_qr_widget_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (widget);
  int texture_size = 0;

  if (self->texture)
    texture_size = gdk_texture_get_width (self->texture);

  *natural = *minimum = MAX (texture_size, self->size);
}

static void
gnome_qr_widget_css_changed (GtkWidget         *widget,
                             GtkCssStyleChange *change)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (widget);
  GdkRGBA fg_color;

  gtk_widget_get_color (widget, &fg_color);

  if (!gdk_rgba_equal (&fg_color, &self->fg_color))
    {
      self->fg_color = fg_color;
      gnome_qr_widget_update (GNOME_QR_WIDGET (widget));
    }

  GTK_WIDGET_CLASS (gnome_qr_widget_parent_class)->css_changed (widget, change);
}

static void
gnome_qr_widget_class_init (GnomeQrWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gnome_qr_widget_dispose;
  object_class->finalize = gnome_qr_widget_finalize;
  object_class->get_property = gnome_qr_widget_get_property;
  object_class->set_property = gnome_qr_widget_set_property;

  widget_class->snapshot = gnome_qr_widget_snapshot;
  widget_class->measure = gnome_qr_widget_measure;
  widget_class->css_changed = gnome_qr_widget_css_changed;

  /**
   * GnomeQrWidget::changed:
   * @self: the #GnomeQrWidget
   *
   * Emitted when the QR code texture has been regenerated.
   *
   * This signal is emitted after the widget's text or error correction
   * level changes and the new QR code has been successfully  generated.
   */
  signals[SIGNAL_CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT);

  properties[PROP_ALTERNATIVE_TEXT] =
    g_param_spec_string ("alternative-text", NULL, NULL,
                         _("QR Code"),
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT);

  properties[PROP_SIZE] =
    g_param_spec_uint64 ("size", NULL, NULL,
                         0, G_MAXSIZE, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT);

  properties[PROP_ECC_LEVEL] =
    g_param_spec_enum ("ecc-level", NULL, NULL,
                       gnome_qr_ecc_level_get_type (),
                       GNOME_QR_ECC_LEVEL_MEDIUM,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gnome-qr-code");

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
gnome_qr_widget_init (GnomeQrWidget *self)
{
}

/**
 * gnome_qr_widget_new:
 * @text: (nullable): the text to encode in the QR code
 *
 * Creates a new #GnomeQrWidget.
 *
 * Returns: (transfer full): a new #GnomeQrWidget
 */
GtkWidget *
gnome_qr_widget_new (const char *text)
{
  return g_object_new (GNOME_TYPE_QR_WIDGET,
                       "text", text,
                       NULL);
}

static void
on_qr_code_generated (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GnomeQrWidget *self = GNOME_QR_WIDGET (user_data);
  g_autoptr (GBytes) qr_data = NULL;
  g_autoptr (GError) error = NULL;
  size_t pixel_size;

  qr_data = gnome_qr_generate_qr_code_finish (result, &pixel_size, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to generate QR code: %s", error->message);

      return;
    }

  g_clear_object (&self->texture);
  self->texture = gdk_memory_texture_new (pixel_size,
                                          pixel_size,
                                          GDK_MEMORY_R8G8B8A8,
                                          qr_data,
                                          pixel_size *
                                          GNOME_QR_BYTES_PER_FORMAT (GNOME_QR_PIXEL_FORMAT_RGBA_8888));

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
}

static void
gnome_qr_widget_update (GnomeQrWidget *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (!self->text || !*self->text)
    {
      g_clear_object (&self->texture);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
      return;
    }

  self->cancellable = g_cancellable_new ();

  /* We use a fully transparent BG color here.
   * So it can be changed via CSS properties without having to
   * regenerate the QR code.
   */
  GnomeQrColor bg_color = { 0 };
  GnomeQrColor fg_color = { 0, 0, 0, 255 };

  fg_color.red = (guint8) (self->fg_color.red * 255);
  fg_color.green = (guint8) (self->fg_color.green * 255);
  fg_color.blue = (guint8) (self->fg_color.blue * 255);
  fg_color.alpha = (guint8) (self->fg_color.alpha * 255);

  gnome_qr_generate_qr_code_async (self->text,
                                   0,
                                   &bg_color,
                                   &fg_color,
                                   GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                   self->ecc_level,
                                   self->cancellable,
                                   on_qr_code_generated,
                                   self);
}

/**
 * gnome_qr_widget_set_text:
 * @self: a #GnomeQrWidget
 * @text: (nullable): the text to encode in the QR code
 *
 * Sets the text to be encoded in the QR code.
 *
 * If @text is %NULL or empty, the QR code will not be displayed.
 */
void
gnome_qr_widget_set_text (GnomeQrWidget *self,
                          const char    *text)
{
  g_return_if_fail (GNOME_IS_QR_WIDGET (self));

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_set_str (&self->text, text);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);

  gnome_qr_widget_update (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
                                  text ? text : _("QR code value not set"),
                                  -1);
}

/**
 * gnome_qr_widget_get_text:
 * @self: a #GnomeQrWidget
 *
 * Gets the text currently encoded in the QR code.
 *
 * Returns: (nullable): the text being encoded, or %NULL if none is set
 */
const char *
gnome_qr_widget_get_text (GnomeQrWidget *self)
{
  g_return_val_if_fail (GNOME_IS_QR_WIDGET (self), NULL);

  return self->text;
}

/**
 * gnome_qr_widget_set_alternative_text:
 * @self: a #GnomeQrWidget
 * @alternative_text: (nullable): the alternative text description for the QR code
 *
 * Sets an alternative textual description for the QR Code contents.
 *
 * It is equivalent to the “alt” attribute for images on websites.
 *
 * This text will be made available to accessibility tools.
 *
 * If the QR code cannot be described textually, set this property to NULL.
 *
 * By default, this is set to "QR Code".
 */
void
gnome_qr_widget_set_alternative_text (GnomeQrWidget *self,
                                      const char    *alternative_text)
{
  g_return_if_fail (GNOME_IS_QR_WIDGET (self));

  if (g_strcmp0 (self->alternative_text, alternative_text) == 0)
    return;

  g_set_str (&self->alternative_text, alternative_text);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                  alternative_text ?
                                  alternative_text : _("QR Code"), -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALTERNATIVE_TEXT]);
}

/**
 * gnome_qr_widget_get_alternative_text:
 * @self: a #GnomeQrWidget
 *
 * Gets the alternative textual description of the QR code.
 *
 * The returned string will be %NULL if the qr code cannot be described textually.
 *
 * Returns: (nullable) (transfer none): the alternative textual description of @self.
 */
const char *
gnome_qr_widget_get_alternative_text (GnomeQrWidget *self)
{
  g_return_val_if_fail (GNOME_IS_QR_WIDGET (self), NULL);

  return self->alternative_text;
}

/**
 * gnome_qr_widget_set_size:
 * @self: a #GnomeQrWidget
 * @size: the size in pixels for the widget's natural and minimum size
 *
 * Sets the natural and minimum size (width and height) of the widget.
 *
 * The QR code will be rendered to fit within this size while
 * maintaining its aspect ratio (square).
 *
 * If the size is smaller than the minimum size of the QR code, it will
 * be ignored and the QR code will be rendered at its minimum size
 * (1 pixel per block).
 */
void
gnome_qr_widget_set_size (GnomeQrWidget *self,
                          size_t         size)
{
  g_return_if_fail (GNOME_IS_QR_WIDGET (self));

  if (self->size == size)
    return;

  self->size = size;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIZE]);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gnome_qr_widget_get_size:
 * @self: a #GnomeQrWidget
 *
 * Gets the current natural size of the widget.
 *
 * Returns: the size in pixels
 */
size_t
gnome_qr_widget_get_size (GnomeQrWidget *self)
{
  g_return_val_if_fail (GNOME_IS_QR_WIDGET (self), 0);

  return self->size;
}

/**
 * gnome_qr_widget_set_ecc_level:
 * @self: a #GnomeQrWidget
 * @ecc: the error correction level
 *
 * Sets the error correction level for the QR code.
 *
 * Higher error correction levels provide better recovery from damage
 * or obscured areas, but result in larger QR codes with more modules.
 */
void
gnome_qr_widget_set_ecc_level (GnomeQrWidget   *self,
                               GnomeQrEccLevel  ecc)
{
  g_return_if_fail (GNOME_IS_QR_WIDGET (self));

  if (self->ecc_level == ecc)
    return;

  self->ecc_level = ecc;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ECC_LEVEL]);

  gnome_qr_widget_update (self);
}

/**
 * gnome_qr_widget_get_ecc_level:
 * @self: a #GnomeQrWidget
 *
 * Gets the current error correction level of the QR code.
 *
 * Returns: the error correction level
 */
GnomeQrEccLevel
gnome_qr_widget_get_ecc_level (GnomeQrWidget *self)
{
  g_return_val_if_fail (GNOME_IS_QR_WIDGET (self), GNOME_QR_ECC_LEVEL_MEDIUM);

  return self->ecc_level;
}
