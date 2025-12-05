/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2025 Canonical Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <gnome-qr/gnome-qr.h>


/* FIXME: Ideally we want to check against visible PNGs so that we can verify
 * the actual rendering, but for now we just compare the raw data because
 * GNOME OS image in CI does not seem to provide a proper png decoder.
 */
static void
assert_qr_matches_reference (GBytes             *qr_code,
                             const char         *reference_file,
                             GnomeQrPixelFormat  format)
{
  g_autoptr (GError) error = NULL;
  g_autofree guint8 *data = NULL;
  gsize size;

#ifdef UPDATE_REFERENCE_DATA
  g_file_set_contents (reference_file,
                       (const gchar *)g_bytes_get_data (qr_code, NULL),
                       g_bytes_get_size (qr_code), &error);
  g_assert_no_error (error);
#endif

  g_file_get_contents (reference_file,
                       (gchar **) &data,
                       &size,
                       &error);
  g_assert_no_error (error);

  g_assert_cmpmem (g_bytes_get_data (qr_code, NULL),
                   g_bytes_get_size (qr_code),
                   data,
                   size);
}

static void
test_gnome_qr_sync_generate_simple_rgb_888 (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) qr_code = NULL;
  size_t pixel_size;

  qr_code = gnome_qr_generate_qr_code_sync ("https://gnome.org",
                                            0, NULL, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            NULL, &error);
  g_assert_nonnull (qr_code);
  g_assert_no_error (error);

  assert_qr_matches_reference (qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-gnome.org-0-rgb.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGB_888);
}

static void
test_gnome_qr_sync_generate_simple_rgba_8888 (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) qr_code = NULL;
  size_t pixel_size;

  qr_code = gnome_qr_generate_qr_code_sync ("https://gnome.org",
                                            0, NULL, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                            GNOME_QR_ECC_LEVEL_LOW,
                                            &pixel_size,
                                            NULL, &error);
  g_assert_nonnull (qr_code);
  g_assert_no_error (error);

  assert_qr_matches_reference (qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-gnome.org-0-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);
}

typedef struct {
  GMainLoop *loop;
  GBytes *qr_code;
  size_t pixel_size;
  GError *error;
} AsyncData;

static void
on_qr_code_generated (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  AsyncData *data = user_data;

  data->qr_code = gnome_qr_generate_qr_code_finish (result,
                                                    &data->pixel_size,
                                                    &data->error);
  g_main_loop_quit (data->loop);
}

static void
test_gnome_qr_async_generate_simple_rgb_888 (void)
{
  AsyncData data = { 0 };
  data.loop = g_main_loop_new (NULL, FALSE);

  gnome_qr_generate_qr_code_async ("https://gnome.org",
                                   0, NULL, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   NULL,
                                   on_qr_code_generated,
                                   &data);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_nonnull (data.qr_code);
  g_assert_no_error (data.error);
  g_assert_cmpuint (data.pixel_size, >, 0);

  assert_qr_matches_reference (data.qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-gnome.org-0-rgb.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGB_888);

  g_bytes_unref (data.qr_code);
}

static void
test_gnome_qr_async_generate_simple_rgba_8888 (void)
{
  AsyncData data = { 0 };
  data.loop = g_main_loop_new (NULL, FALSE);

  gnome_qr_generate_qr_code_async ("https://gnome.org",
                                   0, NULL, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                   GNOME_QR_ECC_LEVEL_LOW,
                                   NULL,
                                   on_qr_code_generated,
                                   &data);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_nonnull (data.qr_code);
  g_assert_no_error (data.error);
  g_assert_cmpuint (data.pixel_size, >, 0);

  assert_qr_matches_reference (data.qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-gnome.org-0-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);

  g_bytes_unref (data.qr_code);
}

static void
test_gnome_qr_sync_generate_custom_colors_fedora (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) qr_code = NULL;
  GnomeQrColor bg_color = { 60, 110, 180, 255 }; /* Fedora blue */
  GnomeQrColor fg_color = { 255, 255, 255, 125 }; /* White with transparency */
  size_t pixel_size;

  qr_code = gnome_qr_generate_qr_code_sync ("https://fedoraproject.org",
                                            256, &bg_color, &fg_color,
                                            GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                            GNOME_QR_ECC_LEVEL_MEDIUM,
                                            &pixel_size,
                                            NULL, &error);
  g_assert_nonnull (qr_code);
  g_assert_no_error (error);
  g_assert_cmpuint (pixel_size, >=, 256);

  assert_qr_matches_reference (qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-fedoraproject.org-256-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);
}

static void
test_gnome_qr_sync_generate_custom_colors_ubuntu (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) qr_code = NULL;
  GnomeQrColor bg_color = { 0, 0, 0, 200 }; /* Black with transparency */
  GnomeQrColor fg_color = { 233, 84, 32, 255 }; /* Ubuntu orange */
  size_t pixel_size;

  qr_code = gnome_qr_generate_qr_code_sync ("https://ubuntu.com",
                                            256, &bg_color, &fg_color,
                                            GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                            GNOME_QR_ECC_LEVEL_QUARTILE,
                                            &pixel_size,
                                            NULL, &error);
  g_assert_nonnull (qr_code);
  g_assert_no_error (error);
  g_assert_cmpuint (pixel_size, >=, 256);

  assert_qr_matches_reference (qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-ubuntu.com-256-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);
}

static void
test_gnome_qr_async_generate_custom_colors_fedora (void)
{
  AsyncData data = { 0 };
  GnomeQrColor bg_color = { 60, 110, 180, 255 }; /* Fedora blue */
  GnomeQrColor fg_color = { 255, 255, 255, 125 }; /* White with transparency */

  data.loop = g_main_loop_new (NULL, FALSE);

  gnome_qr_generate_qr_code_async ("https://fedoraproject.org",
                                   256, &bg_color, &fg_color,
                                   GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                   GNOME_QR_ECC_LEVEL_MEDIUM,
                                   NULL,
                                   on_qr_code_generated,
                                   &data);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_nonnull (data.qr_code);
  g_assert_no_error (data.error);
  g_assert_cmpuint (data.pixel_size, >=, 256);

  assert_qr_matches_reference (data.qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-fedoraproject.org-256-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);

  g_bytes_unref (data.qr_code);
}

static void
test_gnome_qr_async_generate_custom_colors_ubuntu (void)
{
  AsyncData data = { 0 };
  GnomeQrColor bg_color = { 0, 0, 0, 200 }; /* Black with transparency */
  GnomeQrColor fg_color = { 233, 84, 32, 255 }; /* Ubuntu orange */

  data.loop = g_main_loop_new (NULL, FALSE);

  gnome_qr_generate_qr_code_async ("https://ubuntu.com",
                                   256, &bg_color, &fg_color,
                                   GNOME_QR_PIXEL_FORMAT_RGBA_8888,
                                   GNOME_QR_ECC_LEVEL_QUARTILE,
                                   NULL,
                                   on_qr_code_generated,
                                   &data);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_nonnull (data.qr_code);
  g_assert_no_error (data.error);
  g_assert_cmpuint (data.pixel_size, >=, 256);

  assert_qr_matches_reference (data.qr_code,
                               g_test_get_filename (G_TEST_DIST,
                                                    "data",
                                                    "qr-ubuntu.com-256-rgba.data",
                                                    NULL),
                               GNOME_QR_PIXEL_FORMAT_RGBA_8888);

  g_bytes_unref (data.qr_code);
}

static void
test_gnome_qr_sync_generate_cancelled (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GBytes) qr_code = NULL;
  size_t pixel_size;

  cancellable = g_cancellable_new ();
  g_cancellable_cancel (cancellable);

  qr_code = gnome_qr_generate_qr_code_sync ("https://gnome.org",
                                            0, NULL, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            cancellable, &error);

  g_assert_null (qr_code);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

static void
test_gnome_qr_async_generate_cancelled (void)
{
  AsyncData data = { 0 };
  g_autoptr (GCancellable) cancellable = NULL;

  data.loop = g_main_loop_new (NULL, FALSE);
  cancellable = g_cancellable_new ();

  gnome_qr_generate_qr_code_async ("https://gnome.org",
                                   0, NULL, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   cancellable,
                                   on_qr_code_generated,
                                   &data);

  g_cancellable_cancel (cancellable);

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_null (data.qr_code);
  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  g_error_free (data.error);
}

static void
test_gnome_qr_sync_generate_null_text (void)
{
  g_autoptr (GBytes) qr_code = NULL;
  size_t pixel_size;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*text != NULL*");

  qr_code = gnome_qr_generate_qr_code_sync (NULL,
                                            0, NULL, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            NULL, NULL);

  g_test_assert_expected_messages ();
  g_assert_null (qr_code);
}

static void
test_gnome_qr_sync_generate_empty_text (void)
{
  g_autoptr (GBytes) qr_code = NULL;
  size_t pixel_size;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "**text != '\\0'*");

  qr_code = gnome_qr_generate_qr_code_sync ("",
                                            0, NULL, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            NULL, NULL);

  g_test_assert_expected_messages ();
  g_assert_null (qr_code);
}

static void
test_gnome_qr_sync_generate_rgb_with_transparent_bg (void)
{
  g_autoptr (GBytes) qr_code = NULL;
  GnomeQrColor bg_color = { 255, 255, 255, 200 }; /* transparent white */
  size_t pixel_size;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*!bg_color || bg_color->alpha == 255*");

  qr_code = gnome_qr_generate_qr_code_sync ("https://gnome.org",
                                            0, &bg_color, NULL,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            NULL, NULL);

  g_test_assert_expected_messages ();
  g_assert_null (qr_code);
}

static void
test_gnome_qr_sync_generate_rgb_with_transparent_fg (void)
{
  g_autoptr (GBytes) qr_code = NULL;
  GnomeQrColor fg_color = { 0, 0, 0, 200 }; /* transparent black */
  size_t pixel_size;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*!fg_color || fg_color->alpha == 255*");

  qr_code = gnome_qr_generate_qr_code_sync ("https://gnome.org",
                                            0, NULL, &fg_color,
                                            GNOME_QR_PIXEL_FORMAT_RGB_888,
                                            GNOME_QR_ECC_LEVEL_HIGH,
                                            &pixel_size,
                                            NULL, NULL);

  g_test_assert_expected_messages ();
  g_assert_null (qr_code);
}

static void
test_gnome_qr_async_generate_null_text (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*text != NULL*");

  gnome_qr_generate_qr_code_async (NULL,
                                   0, NULL, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   NULL,
                                   NULL,
                                   NULL);

  g_test_assert_expected_messages ();
}

static void
test_gnome_qr_async_generate_empty_text (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "**text != '\\0'*");

  gnome_qr_generate_qr_code_async ("",
                                   0, NULL, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   NULL,
                                   NULL,
                                   NULL);

  g_test_assert_expected_messages ();
}

static void
test_gnome_qr_async_generate_rgb_with_transparent_bg (void)
{
  GnomeQrColor bg_color = { 255, 255, 255, 200 }; /* transparent white */

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*!bg_color || bg_color->alpha == 255*");

  gnome_qr_generate_qr_code_async ("https://gnome.org",
                                   0, &bg_color, NULL,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   NULL,
                                   NULL,
                                   NULL);

  g_test_assert_expected_messages ();
}

static void
test_gnome_qr_async_generate_rgb_with_transparent_fg (void)
{
  GnomeQrColor fg_color = { 0, 0, 0, 200 }; /* transparent black */

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*!fg_color || fg_color->alpha == 255*");

  gnome_qr_generate_qr_code_async ("https://gnome.org",
                                   0, NULL, &fg_color,
                                   GNOME_QR_PIXEL_FORMAT_RGB_888,
                                   GNOME_QR_ECC_LEVEL_HIGH,
                                   NULL,
                                   NULL,
                                   NULL);

  g_test_assert_expected_messages ();
}

int
main (int   argc,
      char *argv[])
{
  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/gnome-qr/sync/generate-simple-rgb-888", test_gnome_qr_sync_generate_simple_rgb_888);
  g_test_add_func ("/gnome-qr/sync/generate-simple-rgba-8888", test_gnome_qr_sync_generate_simple_rgba_8888);
  g_test_add_func ("/gnome-qr/sync/generate-custom-colors-fedora", test_gnome_qr_sync_generate_custom_colors_fedora);
  g_test_add_func ("/gnome-qr/sync/generate-custom-colors-ubuntu", test_gnome_qr_sync_generate_custom_colors_ubuntu);
  g_test_add_func ("/gnome-qr/sync/generate-cancelled", test_gnome_qr_sync_generate_cancelled);
  g_test_add_func ("/gnome-qr/sync/generate-null-text", test_gnome_qr_sync_generate_null_text);
  g_test_add_func ("/gnome-qr/sync/generate-empty-text", test_gnome_qr_sync_generate_empty_text);
  g_test_add_func ("/gnome-qr/sync/generate-rgb-with-transparent-bg", test_gnome_qr_sync_generate_rgb_with_transparent_bg);
  g_test_add_func ("/gnome-qr/sync/generate-rgb-with-transparent-fg", test_gnome_qr_sync_generate_rgb_with_transparent_fg);
  g_test_add_func ("/gnome-qr/async/generate-simple-rgb-888", test_gnome_qr_async_generate_simple_rgb_888);
  g_test_add_func ("/gnome-qr/async/generate-simple-rgba-8888", test_gnome_qr_async_generate_simple_rgba_8888);
  g_test_add_func ("/gnome-qr/async/generate-custom-colors-fedora", test_gnome_qr_async_generate_custom_colors_fedora);
  g_test_add_func ("/gnome-qr/async/generate-custom-colors-ubuntu", test_gnome_qr_async_generate_custom_colors_ubuntu);
  g_test_add_func ("/gnome-qr/async/generate-cancelled", test_gnome_qr_async_generate_cancelled);
  g_test_add_func ("/gnome-qr/async/generate-null-text", test_gnome_qr_async_generate_null_text);
  g_test_add_func ("/gnome-qr/async/generate-empty-text", test_gnome_qr_async_generate_empty_text);
  g_test_add_func ("/gnome-qr/async/generate-rgb-with-transparent-bg", test_gnome_qr_async_generate_rgb_with_transparent_bg);
  g_test_add_func ("/gnome-qr/async/generate-rgb-with-transparent-fg", test_gnome_qr_async_generate_rgb_with_transparent_fg);

  return g_test_run ();
}
