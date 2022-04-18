dump_syms=`find ~/Library/Developer/Xcode/DerivedData  | grep "Build/Products/Release/dump_syms" | head -n1`
symupload=`find ~/Library/Developer/Xcode/DerivedData  | grep "Build/Products/Release/symupload" | head -n1`
echo "dump_syms at: " $dump_syms
echo "symupload at: " $symupload

app="${2}/${4}.app/Contents/MacOS/${4}"
dSYM="${4}.app.dSYM"
sym="${4}.sym"
url="https://${3}.bugsplat.com/post/bp/symbol/breakpadsymbols.php?appName=${4}&appVer=${5}"

eval "${dump_syms} -g ${dSYM} ${app} > ${sym}"
eval $"${symupload} \"${sym}\" \"${url}\""
