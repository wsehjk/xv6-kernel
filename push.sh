#! /usr/bin/bash

cd gitbook_src
gitbook build . book    # output to book directory
cd ..
cp -r gitbook_src/book/* .
git add .
git commit -m "post blog"
echo "*** have been committed, can be pushed to remote ****"