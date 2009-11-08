const struct {
    /*@null@*/ char *command;
} default_config[] = {
{ "set status_format = <span background=\"red\" foreground=\"white\">\\@[\\@keycmd]\\@</span> <b>\\@[\\@TITLE]\\@</b>  - Uzbl browser"},
{ "set title_format_long = \\@keycmd \\@TITLE - Uzbl browser <\\@NAME> > \\@SELECTED_URI"},
{ "set title_format_short = \\@TITLE - Uzbl browser <\\@NAME>"},
{ "set max_conns = 100"}, /* WebkitGTK default: 10 */
{ "set max_conns_host = 6"}, /* WebkitGTK default: 2 */
{ NULL   }
};
