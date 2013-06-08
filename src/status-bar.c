#include "status-bar.h"

/* =========================== PUBLIC API =========================== */

static void
uzbl_status_bar_init (UzblStatusBar *status_bar);

G_DEFINE_TYPE (UzblStatusBar, uzbl_status_bar, GTK_TYPE_BOX)

GtkWidget *
uzbl_status_bar_new ()
{
    return g_object_new (UZBL_TYPE_STATUS_BAR, NULL);
}

void
uzbl_status_bar_update_left (GtkWidget *widget,  const gchar *format)
{
    UzblStatusBar *status_bar = UZBL_STATUS_BAR (widget);

    if (!format || !GTK_IS_LABEL (status_bar->left_label)) {
        return;
    }

    gtk_label_set_markup (GTK_LABEL (status_bar->left_label), format);
}

void
uzbl_status_bar_update_right (GtkWidget *widget, const gchar *format)
{
    UzblStatusBar *status_bar = UZBL_STATUS_BAR (widget);

    if (!format || !GTK_IS_LABEL (status_bar->right_label)) {
        return;
    }

    gtk_label_set_markup (GTK_LABEL (status_bar->right_label), format);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

void
uzbl_status_bar_init (UzblStatusBar *status_bar)
{
    gtk_box_set_homogeneous (GTK_BOX (status_bar), FALSE);
    gtk_box_set_spacing (GTK_BOX (status_bar), 0);

    /* Create left panel. */
    status_bar->left_label = gtk_label_new ("");
    gtk_label_set_selectable (GTK_LABEL (status_bar->left_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (status_bar->left_label), 0, 0);
    gtk_misc_set_padding (GTK_MISC (status_bar->left_label), 2, 2);
    gtk_label_set_ellipsize (GTK_LABEL (status_bar->left_label), PANGO_ELLIPSIZE_END);

    /* Create right panel. */
    status_bar->right_label = gtk_label_new ("");
    gtk_label_set_selectable (GTK_LABEL (status_bar->right_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (status_bar->right_label), 1, 0);
    gtk_misc_set_padding (GTK_MISC (status_bar->right_label), 2, 2);
    gtk_label_set_ellipsize (GTK_LABEL (status_bar->right_label), PANGO_ELLIPSIZE_START);

    /* Add the labels to the status bar. */
    gtk_box_pack_start (GTK_BOX (status_bar), status_bar->left_label,  FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (status_bar), status_bar->right_label, TRUE,  TRUE,  0);
}

static void
allocate (GtkWidget *widget, GtkAllocation *allocation);

void
uzbl_status_bar_class_init (UzblStatusBarClass *class)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    /* Override the size_allocate method. */
    widget_class->size_allocate = allocate;
}

void
allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    GtkRequisition left_requisition;
    GtkRequisition right_requisition;
    GtkAllocation left_allocation;
    GtkAllocation right_allocation;
    UzblStatusBar *status_bar = UZBL_STATUS_BAR (widget);

    int left_natural_width;

#if GTK_CHECK_VERSION (3,0,0)
    GtkRequisition left_requisition_nat;

    gtk_widget_get_preferred_size (status_bar->left_label,  &left_requisition,  &left_requisition_nat);
    gtk_widget_get_preferred_size (status_bar->right_label, &right_requisition, NULL);

    left_natural_width = left_requisition_nat.width;
#else
    gtk_widget_size_request (status_bar->left_label,  &left_requisition);
    gtk_widget_size_request (status_bar->right_label, &right_requisition);

    PangoLayout *left_layout = gtk_label_get_layout (GTK_LABEL (status_bar->left_label));
    pango_layout_get_pixel_size (left_layout, &left_natural_width, NULL);

    /* Some kind of fudge factor seems to be needed here. */
    left_natural_width += 16;
#endif

    gtk_widget_set_allocation (widget, allocation);

    /* The entire allocation, minus the space needed for the right label's ellipsis. */
    int left_max_width = allocation->width - right_requisition.width;

    /* The left label gets max (as much space as it needs, the status bar's allocation). */
    left_allocation.width  = (left_max_width > left_natural_width) ? left_natural_width : left_max_width;

    /* The right label gets whatever is left over. it gets at least enough
     * space for an ellipsis, it seems that it will just display everything if
     * you give it 0. */
    right_allocation.width = allocation->width - left_allocation.width;

    /* Don't fight guys, you can both have as much vertical space as you want! */
    left_allocation.height = right_allocation.height = allocation->height;

    left_allocation.x  = 0;
    right_allocation.x = left_allocation.width;

    left_allocation.y = right_allocation.y = allocation->y;

    gtk_widget_size_allocate (status_bar->left_label,  &left_allocation);
    gtk_widget_size_allocate (status_bar->right_label, &right_allocation);
}
