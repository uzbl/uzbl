/*
 * Uzbl Variables
 */

#ifndef __VARIABLES__
#define __VARIABLES__

#include <glib.h>
#include "type.h"

/* Uzbl variables */

typedef struct {
    enum ptr_type type;
    union {
        int *i;
        float *f;
        gchar **s;
    } ptr;
    int dump;
    int writeable;
    /*@null@*/ void (*func)(void);
} uzbl_cmdprop;

gboolean    set_var_value(const gchar *name, gchar *val);
void        variables_hash();
void        dump_config();
void        dump_config_as_events();

#endif
