/*
 * Uzbl Types
 */

#ifndef __UZBL_TYPE__
#define __UZBL_TYPE__

enum ptr_type {
    TYPE_INT = 1,
    TYPE_STR,
    TYPE_FLOAT,
    TYPE_ULL,
    TYPE_NAME,
    // used by send_event
    TYPE_FORMATTEDSTR,
    TYPE_STR_ARRAY
};

// I'm doing this instead of just using "uzbl_value *" because this way our
// list of variables can be:
//  { .ptr = { .s = &some_char_star }, ... }
// instead of
//  { .ptr = (uzbl_value *)&some_char_star, ... }
// which works here, but I suspect has portability issues.
typedef union uzbl_value_ptr_t {
    int   *i;
    float *f;
    unsigned long long *ull;
    gchar **s;
} uzbl_value_ptr;

/* a really generic function pointer. */
typedef void (*uzbl_fp)(void);

typedef struct {
    enum ptr_type type;
    uzbl_value_ptr ptr;
    int dump;
    int writeable;

    /* the various get_/set_ functions cast these back into something useful. */
    uzbl_fp getter;
    uzbl_fp setter;
} uzbl_cmdprop;

#endif
