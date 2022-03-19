#!/bin/bash
dump_syms="${1}/breakpad/bin/linux/dump_syms"
symupload="${1}/breakpad/bin/linux/sym_upload"
app="${2}/${4}.debug"
sym="${4}.sym"
url="https://${3}.bugsplat.com/post/bp/symbol/breakpadsymbols.php?appName=${4}&appVer=${5}"

eval "${dump_syms} ${app} > ${sym}"
eval $"${symupload} \"${sym}\" \"${url}\""
