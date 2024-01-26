#! /usr/bin/bash

cd gitbook_src
gitbook build
cd ..
cp -r gitbook_src/_book/* .
git add .
git commit -m "post blog"