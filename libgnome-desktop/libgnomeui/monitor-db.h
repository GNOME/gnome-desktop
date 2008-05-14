#ifndef MONITOR_DB_H
#define MONITOR_DB_H

#include <libgnomeui/randrwrap.h>
#include <glib.h>

typedef struct Output Output;
typedef struct Configuration Configuration;

struct Output
{
    char *	name;

    gboolean	on;
    int		width;
    int		height;
    int		rate;
    int		x;
    int		y;
    RWRotation	rotation;

    gboolean	connected;
    char	vendor[4];
    guint	product;
    guint	serial;
    double	aspect;
    int		pref_width;
    int		pref_height;
    char *      display_name;

    gpointer	user_data;
};

struct Configuration
{
    gboolean clone;
    
    Output **outputs;
};

void            configuration_free         (Configuration  *configuration);
Configuration  *configuration_new_current  (RWScreen       *screen);
gboolean        configuration_match        (Configuration  *config1,
					    Configuration  *config2);
gboolean        configuration_save         (Configuration  *configuration,
					    GError        **err);
void		configuration_sanitize     (Configuration  *configuration);
gboolean	configuration_apply_stored (RWScreen       *screen);
gboolean	configuration_applicable   (Configuration  *configuration,
					    RWScreen       *screen);

#endif
