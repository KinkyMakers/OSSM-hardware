#!/bin/bash

GITURL=$(git config --get remote.origin.url)

echo $GITURL

rm -rf html
git clone $GITURL html
cd html
if ( git checkout origin/gh-pages -b gh-pages) then
	echo "Checked out $GITURL gh-pages"
else
	echo "Creating out $GITURL gh-pages"
	git checkout origin/master -b gh-pages
	rm -r *
	echo "# A simple README file for the gh-pages branch" > README.md
	git add README.md
	git commit -m"Replaced gh-pages html with simple readme"
	git push -u origin gh-pages
fi
cd ..

doxygen doxy.doxyfile

cd html
git add * 
git add search/*
git commit -a -m"updating the doxygen"
git push
cd ..
rm -rf html
git checkout master