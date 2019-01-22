#!/usr/bin/bash
# This merges the frontend into a single html to save a few roundtrips to
# the server.

script="$(base64 -w 0 script.js)"
style="$(base64 -w 0 style.css)"


mkdir -p build

cp frontend.html build/index.html
sed -i "s@script\.js@data:script/javascript;base64,$script@; s@style\.css@data:text/css;base64,$style@" build/index.html
