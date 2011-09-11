/*
 * Uzbl Types
 */

enum ptr_type {
    TYPE_INT = 1,
    TYPE_STR,
    TYPE_FLOAT,
    TYPE_NAME,
    // used by send_event
    TYPE_FORMATTEDSTR,
    TYPE_STR_ARRAY
};

typedef struct {
    enum ptr_type type;
    union {
        int   *i;
        float *f;
        gchar **s;
    } ptr;
    int dump;
    int writeable;
    /*@null@*/ void (*func)(void);
} uzbl_cmdprop;

