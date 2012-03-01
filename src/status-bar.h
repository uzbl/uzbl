#ifndef __UZBL_STATUS_BAR_H__
#define __UZBL_STATUS_BAR_H__

#include <gtk/gtk.h>

#define UZBL_TYPE_STATUS_BAR            (uzbl_status_bar_get_type ())
#define UZBL_STATUS_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UZBL_TYPE_STATUS_BAR, UzblStatusBar))
#define UZBL_STATUS_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UZBL_TYPE_STATUS_BAR, UZblStatusBarClass))
#define UZBL_IS_STATUS_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UZBL_TYPE_STATUS_BAR))
#define UZBL_IS_STATUS_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UZBL_TYPE_STATUS_BAR))
#define UZBL_STATUS_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UZBL_TYPE_STATUS_BAR, UzblStatusBarClass))

typedef struct _UzblStatusBar       UzblStatusBar;
typedef struct _UzblStatusBarClass  UzblStatusBarClass;

struct _UzblStatusBar {
    GtkBox  box;

    GtkWidget *left_label;
    GtkWidget *right_label;
};

struct _UzblStatusBarClass {
    GtkBoxClass parent_class;
};

GType       uzbl_status_bar_get_type (void) G_GNUC_CONST;
GtkWidget * uzbl_status_bar_new      ();

void
uzbl_status_bar_update_left(GtkWidget *widget, const gchar *format);

void
uzbl_status_bar_update_right(GtkWidget *widget, const gchar *format);

#endif
