#ifndef UZBL_SETUP_H
#define UZBL_SETUP_H

void
uzbl_commands_init ();
void
uzbl_commands_free ();

void
uzbl_commands_send_builtin_event ();

void
uzbl_events_init ();
void
uzbl_events_free ();

void
uzbl_gui_init ();
void
uzbl_gui_free ();

void
uzbl_inspector_init ();

void
uzbl_io_init ();
void
uzbl_io_free ();
void
uzbl_io_init_stdin ();

void
uzbl_io_flush_buffer ();
void
uzbl_io_quit ();

void
uzbl_js_init ();

void
uzbl_requests_init ();
void
uzbl_requests_free ();

void
uzbl_scheme_init ();

void
uzbl_variables_init ();
void
uzbl_variables_free ();

#endif
