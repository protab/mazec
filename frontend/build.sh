#!/usr/bin/bash
# Tento skript spoji dohromady cely frontend do jednoho HTML,
# aby nebylo nutne delat vice requestu na server.

script="$(base64 -w 0 script.js)"
style="$(base64 -w 0 style.css)"


mkdir -p build

cp frontend.html build/index.html
sed -i "s@script\.js@data:script/javascript;base64,$script@; s@style\.css@data:text/css;base64,$style@" build/index.html
