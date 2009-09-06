const struct {
    /*@null@*/ char *command;
} default_config[] = {
{ "set reset_command_mode = 1"},
{ "set status_format = <span background=\"darkblue\" foreground=\"white\"> \\@MODE </span> <span background=\"red\" foreground=\"white\">\\@[\\@keycmd]\\@</span> (\\@LOAD_PROGRESS%)  <b>\\@[\\@TITLE]\\@</b>  - Uzbl browser"},
{ "set title_format_long = \\@keycmd \\@MODE \\@TITLE - Uzbl browser <\\@NAME> > \\@SELECTED_URI"},
{ "set title_format_short = \\@TITLE - Uzbl browser <\\@NAME>"},
{ "set max_conns = 100"}, /* WebkitGTK default: 10 */
{ "set max_conns_host = 6"}, /* WebkitGTK default: 2 */
{ NULL   }
};
