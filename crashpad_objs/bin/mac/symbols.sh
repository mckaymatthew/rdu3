dump_syms="${1}/crashpad/bin/mac/dump_syms"
symupload="${1}/crashpad/bin/mac/symupload"
app="${2}/${4}.app/Contents/MacOS/${4}"
dSYM="${4}.app.dSYM"
sym="${4}.sym"
url="https://${3}.bugsplat.com/post/bp/symbol/breakpadsymbols.php?appName=${4}&appVer=${5}"

eval "${dump_syms} -g ${dSYM} ${app} > ${sym}"
eval $"${symupload} \"${sym}\" \"${url}\""
