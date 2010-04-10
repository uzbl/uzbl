" Vim filetype file
" Filename:     uzbl.vim
" Maintainer:   Gregor Uhlenheuer
" Last Change:  Sun 04 Apr 2010 01:37:49 PM CEST

if exists('b:did_ftplugin')
  finish
endif

let b:did_ftplugin = 1

" enable syntax based folding
setlocal foldmethod=syntax

" correctly format comments
setlocal formatoptions=croql
setlocal comments=:#
setlocal commentstring=#%s

" define config testing commands and mappings
if executable('uzbl-core')
    com! -buffer UzblCoreTest !uzbl-core -c %
    nmap <buffer> <Leader>uc :UzblCoreTest<CR>
endif

if executable('uzbl-browser')
    com! -buffer UzblBrowserTest !uzbl-browser -c %
    nmap <buffer> <Leader>ub :UzblBrowserTest<CR>
endif
