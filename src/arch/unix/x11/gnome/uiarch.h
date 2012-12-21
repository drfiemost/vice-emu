/*
 * uiarch.h 
 *
 * Written by
 *  pottendo <pottendo@gmx.net>
 *  Oliver Schaertel
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_UIARCH_H
#define VICE_UIARCH_H

/* FIXME: really fix the code for gtk3 */
#undef GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if !GTK_CHECK_VERSION(2, 12, 0)
void gtk_widget_set_tooltip_text(GtkWidget * widget, const char * text);
#endif

/* #define DEBUG_X11UI */
/* #define DEBUGMOUSECURSOR */  /* dont use a blank mouse cursor */
/* #define DEBUGNOMOUSEGRAB */  /* dont grab mouse */
/* #define DEBUG_KBD */
/* #define DEBUGNOKBDGRAB */    /* dont explicitly grab keyboard focus */

#include "vice.h"

#include "log.h"
#include "ui.h"
#include "uiapi.h"

typedef GtkWidget *ui_window_t;
typedef GCallback ui_callback_t;
typedef gpointer ui_callback_data_t;
enum ui_keysym_s {
    KEYSYM_NONE = 0,
    KEYSYM_0 = GDK_0,
    KEYSYM_1 = GDK_1,
    KEYSYM_2 = GDK_2,
    KEYSYM_3 = GDK_3,
    KEYSYM_4 = GDK_4,
    KEYSYM_5 = GDK_5,
    KEYSYM_6 = GDK_6,
    KEYSYM_7 = GDK_7,
    KEYSYM_8 = GDK_8,
    KEYSYM_9 = GDK_9,
    KEYSYM_a = GDK_a,
    KEYSYM_b = GDK_b,
    KEYSYM_c = GDK_c,
    KEYSYM_d = GDK_d,
    KEYSYM_e = GDK_e,
    KEYSYM_f = GDK_f,
    KEYSYM_g = GDK_g,
    KEYSYM_h = GDK_h,
    KEYSYM_i = GDK_i,
    KEYSYM_j = GDK_j,
    KEYSYM_J = GDK_J,
    KEYSYM_k = GDK_k,
    KEYSYM_l = GDK_l,
    KEYSYM_m = GDK_m,
    KEYSYM_n = GDK_n,
    KEYSYM_N = GDK_N,
    KEYSYM_p = GDK_p,
    KEYSYM_q = GDK_q,
    KEYSYM_s = GDK_s,
    KEYSYM_t = GDK_t,
    KEYSYM_u = GDK_u,
    KEYSYM_w = GDK_w,
    KEYSYM_z = GDK_z,
    KEYSYM_F9  = GDK_F9 ,
    KEYSYM_F10 = GDK_F10,
    KEYSYM_F11 = GDK_F11,
    KEYSYM_F12 = GDK_F12
};
typedef enum ui_keysym_s ui_keysym_t;

#define UI_CALLBACK(name) void name(GtkWidget *w, ui_callback_data_t event_data)

#define CHECK_MENUS      (((ui_menu_cb_obj*)event_data)->status != CB_NORMAL)
#define UI_MENU_CB_PARAM (((ui_menu_cb_obj*)event_data)->value) 

/* Drive status widget */
typedef struct {
    GtkWidget *box;                     /* contains all the widgets */
    char *label;
    GtkWidget *pixmap;
#if 0
    GtkWidget *image;
#endif
    GtkWidget *event_box;
    GtkWidget *track_label;
    GtkWidget *led;
    GdkPixmap *led_pixmap;
    GtkWidget *led1;
    GdkPixmap *led1_pixmap;
    GtkWidget *led2;
    GdkPixmap *led2_pixmap;
} drive_status_widget;

/* Tape status widget */
typedef struct {
    GtkWidget *box;
    GtkWidget *event_box;
    GtkWidget *label;
    GtkWidget *control;
    GdkPixmap *control_pixmap;
} tape_status_widget;

#define MAX_APP_SHELLS 10
typedef struct {
    gchar *title;
    GtkWidget *shell;
    GtkWidget *topmenu;
    GtkWidget *status_bar;
    GtkWidget *pal_ctrl;
    void *pal_ctrl_data;
    GtkLabel *speed_label;
    GtkLabel *statustext;
    GtkAccelGroup *accel;
    drive_status_widget drive_status[NUM_DRIVES];
    tape_status_widget tape_status;
    GdkGeometry geo;
    struct video_canvas_s *canvas;
} app_shell_type;
extern app_shell_type app_shells[MAX_APP_SHELLS];
extern int get_num_shells(void);

extern GdkGC *get_toplevel(void);
extern GtkWidget *get_active_toplevel(void);
extern int get_active_shell(void);
extern GdkVisual *visual; /* FIXME: also wrap into a function call */
extern struct video_canvas_s *get_active_canvas(void);

extern void ui_trigger_resize(void);
extern void ui_trigger_window_resize(struct video_canvas_s *c);

extern int ui_open_canvas_window(struct video_canvas_s *c, const char *title, int width, int heigth, int no_autorepeat);
extern void ui_resize_canvas_window(struct video_canvas_s *c);
extern GtkWidget *ui_create_transient_shell(GtkWidget *parent, const char *name);
extern void ui_popdown(GtkWidget *w);
extern void ui_popup(GtkWidget *w, const char *title, gboolean wait_popdown);
extern void ui_make_window_transient(GtkWidget *parent,GtkWidget *window);
extern void ui_about(gpointer data);
extern gint ui_hotkey_event_handler(GtkWidget *w, GdkEvent *report, gpointer gp);
extern void ui_block_shells(void);
extern void ui_unblock_shells(void);
extern int ui_fullscreen_statusbar(struct video_canvas_s *canvas, int enable);

extern void ui_set_drop_callback(void *cb);

extern unsigned char *convert_utf8(unsigned char *s);

/* UI logging goes here.  */
extern log_t ui_log;

/* color constants used by the GUI */
extern GdkColor drive_led_on_red_pixel, drive_led_on_green_pixel;
extern GdkColor drive_led_off_pixel, motor_running_pixel, tape_control_pixel;
extern GdkColor drive_led_on_red_pixels[16];
extern GdkColor drive_led_on_green_pixels[16];

#endif /* !defined (_UIARCH_H) */
