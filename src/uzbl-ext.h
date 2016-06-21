typedef struct _UzblExt UzblExt;

UzblExt*
uzbl_ext_new ();

static void
uzbl_ext_init_io (UzblExt *ext, int in, int out);
