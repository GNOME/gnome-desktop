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

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

/**
 * GNOME_QR_BYTES_PER_FORMAT:
 * @format: a #GnomeQrPixelFormat
 *
 * Calculates the number of bytes per pixel for a given pixel format.
 *
 * Returns: the number of bytes
 */
#define GNOME_QR_BYTES_PER_FORMAT(format) \
        ((format) == GNOME_QR_PIXEL_FORMAT_RGB_888 ? 3 : 4)

/**
 * GNOME_QR_COLOR_WHITE:
 *
 * Predefined white color (255, 255, 255, 255).
 */
#define GNOME_QR_COLOR_WHITE ((GnomeQrColor) { 255, 255, 255, 255 })

/**
 * GNOME_QR_COLOR_BLACK:
 *
 * Predefined black color (0, 0, 0, 255).
 */
#define GNOME_QR_COLOR_BLACK ((GnomeQrColor) { 0, 0, 0, 255 })

/**
 * GnomeQrEccLevel:
 * @GNOME_QR_ECC_LEVEL_LOW: Low error correction (~7% recovery capability)
 * @GNOME_QR_ECC_LEVEL_MEDIUM: Medium error correction (~15% recovery capability)
 * @GNOME_QR_ECC_LEVEL_QUARTILE: Quartile error correction (~25% recovery capability)
 * @GNOME_QR_ECC_LEVEL_HIGH: High error correction (~30% recovery capability)
 *
 * Error correction levels for QR codes. Higher levels provide better error
 * recovery at the cost of requiring bigger dimensions.
 */
typedef enum {
        GNOME_QR_ECC_LEVEL_LOW,
        GNOME_QR_ECC_LEVEL_MEDIUM,
        GNOME_QR_ECC_LEVEL_QUARTILE,
        GNOME_QR_ECC_LEVEL_HIGH,
} GnomeQrEccLevel;

/**
 * GnomeQrPixelFormat:
 * @GNOME_QR_PIXEL_FORMAT_RGB_888: 24-bit RGB format (3 bytes per pixel)
 * @GNOME_QR_PIXEL_FORMAT_RGBA_8888: 32-bit RGBA format (4 bytes per pixel)
 *
 * Pixel formats for the generated QR code image data.
 */
typedef enum {
        GNOME_QR_PIXEL_FORMAT_RGB_888,
        GNOME_QR_PIXEL_FORMAT_RGBA_8888,
} GnomeQrPixelFormat;

/**
 * GnomeQrColor:
 * @red: Red component (0-255)
 * @green: Green component (0-255)
 * @blue: Blue component (0-255)
 * @alpha: Alpha component (0-255, where 255 is fully opaque).
 *   Must be 255 when using %GNOME_QR_PIXEL_FORMAT_RGB_888.
 *
 * Represents an RGBA color for QR code generation.
 */
typedef struct {
        guint8 red;
        guint8 green;
        guint8 blue;
        guint8 alpha;
} GnomeQrColor;

GType       gnome_qr_ecc_level_get_type            (void);

GType       gnome_qr_pixel_format_get_type         (void);

GBytes *    gnome_qr_generate_qr_code_sync         (const char          *text,
                                                    size_t               requested_size,
                                                    const GnomeQrColor  *bg_color,
                                                    const GnomeQrColor  *fg_color,
                                                    GnomeQrPixelFormat   format,
                                                    GnomeQrEccLevel      ecc,
                                                    size_t              *pixel_size_out,
                                                    GCancellable        *cancellable,
                                                    GError             **error);

void        gnome_qr_generate_qr_code_async        (const char          *text,
                                                    size_t               requested_size,
                                                    const GnomeQrColor  *bg_color,
                                                    const GnomeQrColor  *fg_color,
                                                    GnomeQrPixelFormat   format,
                                                    GnomeQrEccLevel      ecc,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data);

GBytes *    gnome_qr_generate_qr_code_finish       (GAsyncResult        *result,
                                                    size_t              *pixel_size_out,
                                                    GError             **error);
