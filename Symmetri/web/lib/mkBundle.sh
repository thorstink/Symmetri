#! /bin/sh
# make use you `npm install -g uglifyjs` first

time uglifyjs d3.v3.min.js lodash.min.js petrinet.js app.js -c -m -o bundle.js

