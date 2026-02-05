/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2019 Purism SPC
 * Copyright 2024-2025 Red Hat, Inc
 * Copyright 2024-2025 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
 * Author: Mohammed Sadiq <sadiq@sadiqpk.org>
 *         Ray Strode <rstrode@redhat.com>
 *         Marco Trevisan <marco.trevisan@canonical.com>
 *         Joan Torres Lopez <joantolo@redhat.com>
 */

#include "config.h"

#include "gnome-qr.h"

#include <math.h>
#include <qrcodegen.h>

G_DEFINE_ENUM_TYPE (GnomeQrEccLevel, gnome_qr_ecc_level,
  G_DEFINE_ENUM_VALUE (GNOME_QR_ECC_LEVEL_LOW, "low"),
  G_DEFINE_ENUM_VALUE (GNOME_QR_ECC_LEVEL_MEDIUM, "medium"),
  G_DEFINE_ENUM_VALUE (GNOME_QR_ECC_LEVEL_QUARTILE, "quartile"),
  G_DEFINE_ENUM_VALUE (GNOME_QR_ECC_LEVEL_HIGH, "high"))

G_DEFINE_ENUM_TYPE (GnomeQrPixelFormat, gnome_qr_pixel_format,
  G_DEFINE_ENUM_VALUE (GNOME_QR_PIXEL_FORMAT_RGB_888, "rgb-888"),
  G_DEFINE_ENUM_VALUE (GNOME_QR_PIXEL_FORMAT_RGBA_8888, "rgba-8888"))

typedef struct
{
        char *text;
        size_t requested_size;
        size_t pixel_size;
        GnomeQrColor *bg_color;
        GnomeQrColor *fg_color;
        GnomeQrPixelFormat format;
        GnomeQrEccLevel ecc;
} GnomeQrCodeData;

static void
gnome_qr_code_data_free (GnomeQrCodeData *data)
{
        g_clear_pointer (&data->text, g_free);
        g_clear_pointer (&data->bg_color, g_free);
        g_clear_pointer (&data->fg_color, g_free);
        g_free (data);
}

static enum qrcodegen_Ecc
gnome_qr_ecc_level_to_qrcodegen_ecc (GnomeQrEccLevel ecc)
{
        switch (ecc) {
        case GNOME_QR_ECC_LEVEL_LOW:
                return qrcodegen_Ecc_LOW;
        case GNOME_QR_ECC_LEVEL_MEDIUM:
                return qrcodegen_Ecc_MEDIUM;
        case GNOME_QR_ECC_LEVEL_QUARTILE:
                return qrcodegen_Ecc_QUARTILE;
        case GNOME_QR_ECC_LEVEL_HIGH:
                return qrcodegen_Ecc_HIGH;
        }
        g_assert_not_reached ();
}

static void
fill_block (GByteArray         *array,
            const GnomeQrColor *color,
            GnomeQrPixelFormat  format,
            int                 block_size)
{
        guint i;

        for (i = 0; i < block_size; i++) {
                g_byte_array_append (array, &color->red, 1);
                g_byte_array_append (array, &color->green, 1);
                g_byte_array_append (array, &color->blue, 1);

                if (format == GNOME_QR_PIXEL_FORMAT_RGBA_8888)
                        g_byte_array_append (array, &color->alpha, 1);
        }
}

G_ALWAYS_INLINE static inline gboolean
check_color_validity (const GnomeQrColor *color,
                      GnomeQrPixelFormat  format)
{
        if (!color)
                return TRUE;

        if (format == GNOME_QR_PIXEL_FORMAT_RGB_888) {
                g_return_val_if_fail (color->alpha == 255, FALSE);
        }

        return TRUE;
}

/**
 * gnome_qr_generate_qr_code_sync:
 * @text: the text of which generate the QR code
 * @requested_size: The requested size (width and height) in pixels of the QR code.
 *   Only square QR codes are supported. If the requested size is smaller than
 *   the minimum required size for the QR code, it will be generated with 1 pixel
 *   per block.
 * @bg_color: (nullable): The background color of the code
 *   or %NULL to use default (white)
 * @fg_color: (nullable): The foreground color of the code
 *   or %NULL to use default (black)
 * @format: The pixel format for the output image data
 * @ecc: The error correction level
 * @pixel_size_out: (out): The actual square QR code size (width and height)
 *   in pixels. Note that it may not match @requested_size.
 * @cancellable: (nullable): A #GCancellable to cancel the operation
 * @error: #GError for error reporting
 *
 * Generates the QrCode synchronously.
 *
 * Returns: (transfer full): The pixel data or %NULL on error
 */
GBytes *
gnome_qr_generate_qr_code_sync (const char          *text,
                                size_t               requested_size,
                                const GnomeQrColor  *bg_color,
                                const GnomeQrColor  *fg_color,
                                GnomeQrPixelFormat   format,
                                GnomeQrEccLevel      ecc,
                                size_t              *pixel_size_out,
                                GCancellable        *cancellable,
                                GError             **error)
{
        g_autoptr (GByteArray) qr_matrix = NULL;
        uint8_t qr_code[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
        uint8_t temp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
        static const GnomeQrColor default_bg_color = GNOME_QR_COLOR_WHITE;
        static const GnomeQrColor default_fg_color = GNOME_QR_COLOR_BLACK;
        gint qr_size, block_size, total_size;
        gint column, row, i;

        g_return_val_if_fail (text != NULL, NULL);
        g_return_val_if_fail (*text != '\0', NULL);
        g_return_val_if_fail (pixel_size_out != NULL, NULL);
        g_return_val_if_fail (check_color_validity (bg_color, format), NULL);
        g_return_val_if_fail (check_color_validity (fg_color, format), NULL);

        if (g_cancellable_set_error_if_cancelled (cancellable, error))
                return NULL;

        if (!qrcodegen_encodeText (text,
                                   temp_buf,
                                   qr_code,
                                   gnome_qr_ecc_level_to_qrcodegen_ecc (ecc),
                                   qrcodegen_VERSION_MIN,
                                   qrcodegen_VERSION_MAX,
                                   qrcodegen_Mask_AUTO,
                                   FALSE)) {
                g_set_error (error,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "QRCode generation failed for content '%s'",
                             text);
                return NULL;
        }

        if (g_cancellable_set_error_if_cancelled (cancellable, error))
                return NULL;

        if (!bg_color)
                bg_color = &default_bg_color;

        if (!fg_color)
                fg_color = &default_fg_color;

        qr_size = qrcodegen_getSize (qr_code);
        g_assert (qr_size > 0);

        block_size = MAX (1, ceil ((double) requested_size / qr_size));
        total_size = qr_size * block_size;

        qr_matrix = g_byte_array_sized_new (total_size * total_size *
                                            GNOME_QR_BYTES_PER_FORMAT (format));

        for (row = 0; row < qr_size; ++row) {
                for (i = 0; i < block_size; ++i) {
                        for (column = 0; column < qr_size; ++column) {
                                if (qrcodegen_getModule (qr_code, column, row))
                                        fill_block (qr_matrix, fg_color, format, block_size);
                                else
                                        fill_block (qr_matrix, bg_color, format, block_size);
                        }
                }

                if (g_cancellable_set_error_if_cancelled (cancellable, error))
                        return NULL;
        }

        *pixel_size_out = total_size;

        return g_byte_array_free_to_bytes (g_steal_pointer (&qr_matrix));
}


static void
generate_qr_code_in_thread (GTask        *task,
                            gpointer      source,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
        GnomeQrCodeData *data = task_data;
        g_autoptr (GError) error = NULL;
        g_autoptr (GBytes) icon = NULL;

        icon = gnome_qr_generate_qr_code_sync (data->text,
                                               data->requested_size,
                                               data->bg_color,
                                               data->fg_color,
                                               data->format,
                                               data->ecc,
                                               &data->pixel_size,
                                               cancellable,
                                               &error);
        if (!icon) {
                g_task_return_error (task, g_steal_pointer (&error));
                return;
        }

        g_task_return_pointer (task, g_steal_pointer (&icon), (GDestroyNotify) g_bytes_unref);
}

/**
 * gnome_qr_generate_qr_code_async:
 * @text: The text of which generate the QR code
 * @requested_size: The requested size (width and height) in pixels of the QR code.
 *   Only square QR codes are supported. If the requested size is smaller than
 *   the minimum required size for the QR code, it will be generated with 1 pixel
 *   per block.
 * @bg_color: (nullable): The background color of the code
 *   or %NULL to use default (white)
 * @fg_color: (nullable): The foreground color of the code
 *   or %NULL to use default (black)
 * @format: The pixel format for the output image data
 * @ecc: The error correction level
 * @cancellable: (nullable): A #GCancellable to cancel the operation
 * @callback: (scope async): function to call returning success or failure
 *   of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Generates the QrCode asynchronously.
 *
 * Use gnome_qr_generate_qr_code_finish() to complete it.
 */
void
gnome_qr_generate_qr_code_async (const char          *text,
                                 size_t               requested_size,
                                 const GnomeQrColor  *bg_color,
                                 const GnomeQrColor  *fg_color,
                                 GnomeQrPixelFormat   format,
                                 GnomeQrEccLevel      ecc,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
        g_autoptr (GTask) task = NULL;
        GnomeQrCodeData *data;

        g_return_if_fail (text != NULL);
        g_return_if_fail (*text != '\0');
        g_return_if_fail (check_color_validity (bg_color, format));
        g_return_if_fail (check_color_validity (fg_color, format));

        data = g_new0 (GnomeQrCodeData, 1);
        data->text = g_strdup (text);
        data->requested_size = requested_size;
        data->bg_color = g_memdup2 (bg_color, sizeof (GnomeQrColor));
        data->fg_color = g_memdup2 (fg_color, sizeof (GnomeQrColor));
        data->format = format;
        data->ecc = ecc;

        task = g_task_new (NULL, cancellable, callback, user_data);
        g_task_set_source_tag (task, gnome_qr_generate_qr_code_async);
        g_task_set_task_data (task, data,
                              (GDestroyNotify) gnome_qr_code_data_free);
        g_task_set_return_on_cancel (task, TRUE);
        g_task_run_in_thread (task, generate_qr_code_in_thread);
}

/**
 * gnome_qr_generate_qr_code_finish:
 * @result: the #GAsyncResult that was provided to the callback
 * @pixel_size_out: (out): The actual square QR code size (width and height)
 *   in pixels. Note that it may not match the @requested_size in
 *   gnome_qr_generate_qr_code_async().
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by
 * gnome_qr_generate_qr_code_async() and obtain its result.
 *
 * Returns: (transfer full): The pixel data or %NULL on error
 */
GBytes *
gnome_qr_generate_qr_code_finish (GAsyncResult  *result,
                                  size_t        *pixel_size_out,
                                  GError       **error)
{
        GnomeQrCodeData *data;
        g_autoptr (GBytes) pixel_data = NULL;

        g_return_val_if_fail (G_IS_TASK (result), FALSE);
        g_return_val_if_fail (g_async_result_is_tagged (result,
                                                        gnome_qr_generate_qr_code_async),
                              NULL);
        g_return_val_if_fail (pixel_size_out != NULL, NULL);

        pixel_data = g_task_propagate_pointer (G_TASK (result), error);
        if (!pixel_data)
                return NULL;

        data = g_task_get_task_data (G_TASK (result));
        *pixel_size_out = data->pixel_size;

        return g_steal_pointer (&pixel_data);
}
