" Vim syntax file
" Language:     Uzbl config syntax
" Maintainer:   Gregor Uhlenheuer (kongo2002) <kongo2002@gmail.com>
" Contributors: Mason Larobina <mason.larobina@gmail.com>
"               Pawel Tomak (grodzik) <pawel.tomak@gmail.com>
" Version:      0.1
"
" Installing:
" To install this syntax file place it in your `~/.vim/syntax/` directory.
"
" To enable automatic uzbl-config file type detection create a new file
" `~/.vim/ftdetect/uzbl.vim` with the following lines inside:
"
"    au BufRead,BufNewFile  ~/.config/uzbl/*  set filetype=uzbl
"    au BufRead,BufNewFile  */uzbl/config     set filetype=uzbl
"
" Issues:
"   1. Hilighting inside @[]@, @()@, @<>@ and string regions would be nice.
"   2. Accidentally matches keywords inside strings.
"

if version < 600
    syntax clear
elseif exists("b:current_syntax")
    finish
endif

syn keyword uzblKeyword back forward scroll reload reload_ign_cache stop
syn keyword uzblKeyword zoom_in zoom_out toggle_zoom_type uri script
syn keyword uzblKeyword toggle_status spawn sync_spawn sync_sh sync_spawn_exec
syn keyword uzblKeyword exit search search_reverse search_clear dehilight set
syn keyword uzblKeyword dump_config dump_config_as_events chain print event
syn keyword uzblKeyword request menu_add menu_link_add menu_image_add
syn keyword uzblKeyword menu_editable_add menu_separator menu_link_separator
syn keyword uzblKeyword menu_image_separator menu_editable_separator
syn keyword uzblKeyword menu_remove menu_link_remove menu_image_remove
syn keyword uzblKeyword menu_editable_remove hardcopy include

" Match 'js' and 'sh' only without a dot in front
syn match uzblKeyword /\.\@<!sh\s\+/
syn match uzblKeyword /\.\@<!js\s\+/

" Comments
syn match uzblTodo /TODO:/ contained
syn region uzblComment display start=/^#/ end=/$/ contains=uzblTodo

" Comment headings
syn region uzblSec matchgroup=uzblSection start=/^# ===.*$/ end=/^# ===/me=e-5 contains=ALL fold
syn region uzblSSec matchgroup=uzblSubSection start=/^# ---.*$/ end=/^# [=-]\{3}/me=e-5 contains=ALLBUT,uzblSec,uzblSSec fold

" Integer and float matching
syn match uzblPercent display /\s[+-]\=\%(\d\+\.\)\=\d\+%\_s/
syn match uzblInt display /\s[+-]\=\d\+\_s/
syn match uzblFloat display /\s[+-]\=\d\+\.\d\+\_s/

" Handler arguments
syn match uzblArgs display /$\d\+/

" Hex colors
syn match uzblHexCol display /#\x\{3}\%(\x\{3}\)\=/

" Matches @INTERNAL_VAR and @{INTERNAL_VAR}
syn match uzblInternalExpand display /@[A-Z_]\+/
syn match uzblInternalExpand display /@{[A-Z_]\+}/

" Matches $ENVIRON_VAR
syn match uzblEnvironVariable display /$\a\+\w*/

" Matches @some_var and @{some_var}
syn match uzblExpand display /@[A-Za-z0-9_\.]\+/
syn match uzblExpand display /@{[A-Za-z0-9_\.]\+}/

" Matches @command_alias at the beginning of a line.
syn match uzblFunctExpand display /^@[A-Za-z0-9_\.]\+/
syn match uzblFunctExpand display /^@{[A-Za-z0-9_\.]\+}/

" Matches literal \, \@var and \@{var}
syn match uzblEscape display /\\\\/
syn match uzblEscape display /\\@/
syn match uzblEscape display /\\@[A-Za-z0-9_\.]\+/
syn match uzblEscape display /\\@{[A-Za-z0-9_\.]\+}/

" Match @[ xml data ]@ regions
syn region uzblXMLEscape display start=+@\[+ end=+\]@+ end=+$+
syn region uzblEscape start=+\\@\[+ end=+\]\\@+

" Match @( shell command )@ regions
syn region uzblShellExec display start=+@(+ end=+)@+ end=+$+
syn region uzblEscape start=+\\@(+ end=+)\\@+

" Match @< javascript command >@ regions
syn region uzblJSExec display start=+@<+ end=+>@+ end=+$+
syn region uzblEscape start=+\\@<+ end=+>\\@+

" Match quoted regions
syn region uzblString display start=+'+ end=+'+ end=+$+ contains=uzblExpand,uzblEscape,uzblHexCol,uzblArgs,uzblInternalExpand,uzblEnvironVariable,uzblXMLEscape,uzblShellExec,uzblJSExec
syn region uzblString display start=+"+ end=+"+ end=+$+ contains=uzblExpand,uzblEscape,uzblHexCol,uzblArgs,uzblInternalExpand,uzblEnvironVariable,uzblXMLEscape,uzblShellExec,uzblJSExec

if version >= 508 || !exists("did_uzbl_syn_inits")
    if version <= 508
        let did_uzbl_syn_inits = 1
        command -nargs=+ HiLink hi link <args>
    else
        command -nargs=+ HiLink hi def link <args>
    endif

    HiLink uzblComment Comment
    HiLink uzblTodo Todo

    HiLink uzblSection SpecialComment
    HiLink uzblSubSection SpecialComment

    HiLink uzblKeyword Keyword

    HiLink uzblInt Number
    HiLink uzblPercent Number
    HiLink uzblFloat Float

    HiLink uzblHexCol Constant

    HiLink uzblArgs Identifier

    HiLink uzblExpand Type
    HiLink uzblFunctExpand Macro
    HiLink uzblEnvironVariable Number
    HiLink uzblInternalExpand Identifier

    HiLink uzblXMLEscape Macro
    HiLink uzblShellExec Macro
    HiLink uzblJSExec Macro

    HiLink uzblEscape Special

    HiLink uzblString String

    delcommand HiLink
endif

let b:current_syntax = 'uzbl'
