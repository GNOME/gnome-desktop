/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <locale.h>
#include <gtk/gtk.h>
#include <gnome-qr-gtk/gnome-qr-widget.h>

static void
on_size_changed (GtkSpinButton *spin,
                 gpointer       user_data)
{
  GnomeQrWidget *qr_widget = GNOME_QR_WIDGET (user_data);
  int value = gtk_spin_button_get_value_as_int (spin);
  gnome_qr_widget_set_size (qr_widget, value);
}

static void
on_fg_color_changed (GtkColorDialogButton *button,
                     GParamSpec           *pspec,
                     gpointer              user_data)
{
  GtkWidget *qr_widget = GTK_WIDGET (user_data);
  GtkCssProvider *provider;
  const GdkRGBA *color;
  g_autofree char *css = NULL;

  color = gtk_color_dialog_button_get_rgba (button);
  css = g_strdup_printf ("gnome-qr-code { color: rgba(%d, %d, %d, %.2f); }",
                         (int)(color->red * 255),
                         (int)(color->green * 255),
                         (int)(color->blue * 255),
                         color->alpha);

  provider = g_object_get_data (G_OBJECT (qr_widget), "fg-css-provider");
  gtk_css_provider_load_from_string (provider, css);
}

static void
on_bg_color_changed (GtkColorDialogButton *button,
                     GParamSpec           *pspec,
                     gpointer              user_data)
{
  GtkWidget *qr_widget = GTK_WIDGET (user_data);
  GtkCssProvider *provider;
  const GdkRGBA *color;
  g_autofree char *css = NULL;

  color = gtk_color_dialog_button_get_rgba (button);
  css = g_strdup_printf ("gnome-qr-code { background-color: rgba(%d, %d, %d, %.2f); }",
                         (int)(color->red * 255),
                         (int)(color->green * 255),
                         (int)(color->blue * 255),
                         color->alpha);

  provider = g_object_get_data (G_OBJECT (qr_widget), "bg-css-provider");
  gtk_css_provider_load_from_string (provider, css);
}

static void
on_activate (GtkApplication *app,
             gpointer        user_data)
{
  GtkWidget *window;
  GtkWidget *overlay;
  GtkWidget *qr_widget;
  GtkWidget *controls_box;
  GtkWidget *text_entry;
  GtkWidget *size_spin;
  GtkWidget *ecc_dropdown;
  GtkWidget *colors_box;
  GtkWidget *fg_color_button;
  GtkWidget *bg_color_button;
  GtkCssProvider *fg_css_provider;
  GtkCssProvider *bg_css_provider;
  GdkRGBA fg_color = { 0.2, 0.4, 0.8, 0.85 };  /* Adwaita Blue */
  GdkRGBA bg_color = { 0.8, 0.8, 0.8, 1.0 };  /* Light gray */
  const char *ecc_levels[] = { "Low", "Medium", "Quartile", "High", NULL };

  setlocale (LC_ALL, "C.UTF-8");

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "QR Code Widget Example");
  gtk_window_set_default_size (GTK_WINDOW (window), 900, 500);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  qr_widget = gnome_qr_widget_new (NULL);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), qr_widget);

  fg_css_provider = gtk_css_provider_new ();
  bg_css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (fg_css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (bg_css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_set_data_full (G_OBJECT (qr_widget), "fg-css-provider",
                          fg_css_provider, g_object_unref);
  g_object_set_data_full (G_OBJECT (qr_widget), "bg-css-provider",
                          bg_css_provider, g_object_unref);

  controls_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign (controls_box, GTK_ALIGN_START);
  gtk_widget_set_valign (controls_box, GTK_ALIGN_START);
  gtk_widget_set_margin_start (controls_box, 12);
  gtk_widget_set_margin_top (controls_box, 12);
  gtk_widget_add_css_class (controls_box, "toolbar");
  gtk_widget_add_css_class (controls_box, "osd");
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), controls_box);

  text_entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (text_entry), "https://www.gnome.org");
  gtk_entry_set_placeholder_text (GTK_ENTRY (text_entry), "Enter text to encode");
  gtk_widget_set_tooltip_text (text_entry, "Text to encode in QR code");
  g_object_bind_property (text_entry, "text",
                          qr_widget, "text",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (controls_box), text_entry);

  size_spin = gtk_spin_button_new_with_range (0, 1000, 10);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (size_spin), 300);
  gtk_widget_set_tooltip_text (size_spin, "Widget size in pixels");
  g_signal_connect (size_spin, "value-changed", G_CALLBACK (on_size_changed), qr_widget);
  gtk_box_append (GTK_BOX (controls_box), size_spin);

  ecc_dropdown = gtk_drop_down_new_from_strings (ecc_levels);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (ecc_dropdown), GNOME_QR_ECC_LEVEL_MEDIUM);
  gtk_widget_set_tooltip_text (ecc_dropdown, "Error Correction Code (ECC) level");
  g_object_bind_property (ecc_dropdown, "selected",
                          qr_widget, "ecc-level",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (controls_box), ecc_dropdown);

  colors_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (colors_box, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (controls_box), colors_box);

  fg_color_button = gtk_color_dialog_button_new (gtk_color_dialog_new ());
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (fg_color_button), &fg_color);
  gtk_widget_set_tooltip_text (fg_color_button, "Foreground color");
  g_signal_connect (fg_color_button, "notify::rgba",
                    G_CALLBACK (on_fg_color_changed), qr_widget);
  on_fg_color_changed (GTK_COLOR_DIALOG_BUTTON (fg_color_button), NULL, qr_widget);
  gtk_box_append (GTK_BOX (colors_box), fg_color_button);

  bg_color_button = gtk_color_dialog_button_new (gtk_color_dialog_new ());
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (bg_color_button), &bg_color);
  gtk_widget_set_tooltip_text (bg_color_button, "Background color");
  g_signal_connect (bg_color_button, "notify::rgba",
                    G_CALLBACK (on_bg_color_changed), qr_widget);
  on_bg_color_changed (GTK_COLOR_DIALOG_BUTTON (bg_color_button), NULL, qr_widget);
  gtk_box_append (GTK_BOX (colors_box), bg_color_button);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  g_autoptr (GtkApplication) app = NULL;

  app = gtk_application_new ("org.gnome.qr-widget-test", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
