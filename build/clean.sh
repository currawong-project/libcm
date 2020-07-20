#!/bin/bash
#
# Run 'make distclean' to clean many of the temporary make files.
# then use this script run from cm/build to clean the remaining files
#



function clean_dir {

    make -C $1 uninstall
    make -C $1 distclean

    
    rm -f  $1/bin/kc.app/Contents/MacOS/kc
    
    rm -rf $1/include
    rm -rf $1/lib
    rm -rf $1/bin
    rm -rf $1/.deps
    
}



clean_dir linux/debug
clean_dir linux/release
clean_dir osx/debug
clean_dir osx/release

rm -rf osx/debug/a.out.dSYM


#rm -rf ../octave/results

# remove all of emacs backup files (files ending width '~')
# find ../ -name "*~" -exec rm {} \; 



