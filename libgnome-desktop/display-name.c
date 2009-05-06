/*
 * Copyright 2007 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Author: Soren Sandmann <sandmann@redhat.com> */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "edid.h"

typedef struct Vendor Vendor;
struct Vendor
{
    const char vendor_id[4];
    const char vendor_name[28];
};

/* This list of vendor codes derived from lshw
 * 
 * http://ezix.org/project/wiki/HardwareLiSter
 */
static const struct Vendor vendors[] = 
{
    { "AIC", "AG Neovo" },
    { "ACR", "Acer" },
    { "DEL", "DELL" },
    { "SAM", "SAMSUNG" },
    { "SNY", "SONY" },
    { "SEC", "Epson" },
    { "WAC", "Wacom" },
    { "NEC", "NEC" },
    { "CMO", "CMO" },	/* Chi Mei */
    { "BNQ", "BenQ" },

    { "ABP", "Advansys" },
    { "ACC", "Accton" },
    { "ACE", "Accton" },
    { "ADP", "Adaptec" },
    { "ADV", "AMD" },
    { "AIR", "AIR" },
    { "AMI", "AMI" },
    { "ASU", "ASUS" },
    { "ATI", "ATI" },
    { "ATK", "Allied Telesyn" },
    { "AZT", "Aztech" },
    { "BAN", "Banya" },
    { "BRI", "Boca Research" },
    { "BUS", "Buslogic" },
    { "CCI", "Cache Computers Inc." },
    { "CHA", "Chase" },
    { "CMD", "CMD Technology, Inc." },
    { "COG", "Cogent" },
    { "CPQ", "Compaq" },
    { "CRS", "Crescendo" },
    { "CSC", "Crystal" },
    { "CSI", "CSI" },
    { "CTL", "Creative Labs" },
    { "DBI", "Digi" },
    { "DEC", "Digital Equipment" },
    { "DBK", "Databook" },
    { "EGL", "Eagle Technology" },
    { "ELS", "ELSA" },
    { "ESS", "ESS" },
    { "FAR", "Farallon" },
    { "FDC", "Future Domain" },
    { "HWP", "Hewlett-Packard" },
    { "IBM", "IBM" },
    { "INT", "Intel" },
    { "ISA", "Iomega" },
    { "LEN", "Lenovo" },
    { "MDG", "Madge" },
    { "MDY", "Microdyne" },
    { "MET", "Metheus" },
    { "MIC", "Micronics" },
    { "MLX", "Mylex" },
    { "NVL", "Novell" },
    { "OLC", "Olicom" },
    { "PRO", "Proteon" },
    { "RII", "Racal" },
    { "RTL", "Realtek" },
    { "SCM", "SCM" },
    { "SKD", "SysKonnect" },
    { "SGI", "SGI" },
    { "SMC", "SMC" },
    { "SNI", "Siemens Nixdorf" },
    { "STL", "Stallion Technologies" },
    { "SUN", "Sun" },
    { "SUP", "SupraExpress" },
    { "SVE", "SVEC" },
    { "TCC", "Thomas-Conrad" },
    { "TCI", "Tulip" },
    { "TCM", "3Com" },
    { "TCO", "Thomas-Conrad" },
    { "TEC", "Tecmar" },
    { "TRU", "Truevision" },
    { "TOS", "Toshiba" },
    { "TYN", "Tyan" },
    { "UBI", "Ungermann-Bass" },
    { "USC", "UltraStor" },
    { "VDM", "Vadem" },
    { "VMI", "Vermont" },
    { "WDC", "Western Digital" },
    { "ZDS", "Zeos" },

    /* From http://faydoc.tripod.com/structures/01/0136.htm */
    { "ACT", "Targa" },
    { "ADI", "ADI" },
    { "AOC", "AOC Intl" },
    { "API", "Acer America" },
    { "APP", "Apple Computer" },
    { "ART", "ArtMedia" },
    { "AST", "AST Research" },
    { "CPL", "Compal" },
    { "CTX", "Chuntex Electronic Co." },
    { "DPC", "Delta Electronics" },
    { "DWE", "Daewoo" },
    { "ECS", "ELITEGROUP" },
    { "EIZ", "EIZO" },
    { "FCM", "Funai" },
    { "GSM", "LG Electronics" },
    { "GWY", "Gateway 2000" },
    { "HEI", "Hyundai" },
    { "HIT", "Hitachi" },
    { "HSL", "Hansol" },
    { "HTC", "Hitachi" },
    { "ICL", "Fujitsu ICL" },
    { "IVM", "Idek Iiyama" },
    { "KFC", "KFC Computek" },
    { "LKM", "ADLAS" },
    { "LNK", "LINK Tech" },
    { "LTN", "Lite-On" },
    { "MAG", "MAG InnoVision" },
    { "MAX", "Maxdata" },
    { "MEI", "Panasonic" },
    { "MEL", "Mitsubishi" },
    { "MIR", "miro" },
    { "MTC", "MITAC" },
    { "NAN", "NANAO" },
    { "NEC", "NEC Tech" },
    { "NOK", "Nokia" },
    { "OQI", "OPTIQUEST" },
    { "PBN", "Packard Bell" },
    { "PGS", "Princeton" },
    { "PHL", "Philips" },
    { "REL", "Relisys" },
    { "SDI", "Samtron" },
    { "SMI", "Smile" },
    { "SPT", "Sceptre" },
    { "SRC", "Shamrock Technology" },
    { "STP", "Sceptre" },
    { "TAT", "Tatung" },
    { "TRL", "Royal Information Company" },
    { "TSB", "Toshiba, Inc." },
    { "UNM", "Unisys" },
    { "VSC", "ViewSonic" },
    { "WTC", "Wen Tech" },
    { "ZCM", "Zenith Data Systems" },

    { "???", "Unknown" },
};

static const char *
find_vendor (const char *code)
{
    int i;

    for (i = 0; i < sizeof (vendors) / sizeof (vendors[0]); ++i)
    {
	const Vendor *v = &(vendors[i]);
	
	if (strcmp (v->vendor_id, code) == 0)
	    return v->vendor_name;
    }

    return code;
};

char *
make_display_name (const char *output_name,
		   const MonitorInfo *info)
{
    const char *vendor;
    int width_mm, height_mm, inches;

    if (output_name &&
	(strstr (output_name, "lvds")	||
	 strstr (output_name, "LVDS")	||
	 strstr (output_name, "Lvds")))
    {
	vendor = _("Laptop");
    }
    else if (info)
    {
	vendor = find_vendor (info->manufacturer_code);
    }
    else
    {
	vendor = _("Unknown");
    }

    if (info && info->width_mm != -1 && info->height_mm)
    {
	width_mm = info->width_mm;
	height_mm = info->height_mm;
    }
    else if (info && info->n_detailed_timings)
    {
	width_mm = info->detailed_timings[0].width_mm;
	height_mm = info->detailed_timings[0].height_mm;
    }
    else
    {
	width_mm = -1;
	height_mm = -1;
    }
    
    if (width_mm != -1 && height_mm != -1)
    {
	double d = sqrt (width_mm * width_mm + height_mm * height_mm);

	inches = (int)(d / 25.4 + 0.5);
    }
    else
    {
	inches = -1;
    }

    if (inches > 0)
	return g_strdup_printf ("%s %d\"", vendor, inches);
    else
	return g_strdup_printf ("%s", vendor);
}
