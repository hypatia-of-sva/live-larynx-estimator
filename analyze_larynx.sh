#!/bin/sh

#see https://stackoverflow.com/questions/394230/how-to-detect-the-os-from-a-bash-script
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PROGRAMNAME=praat
elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
    PROGRAMNAME=Praat
    export PATH=$PATH:/Applications/Praat.app/Contents/macOS
#elif [[ "$OSTYPE" == "cygwin" ]]; then
        # POSIX compatibility layer and Linux environment emulation for Windows
#elif [[ "$OSTYPE" == "msys" ]]; then
#        # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
#elif [[ "$OSTYPE" == "win32" ]]; then
        # I'm not sure this can happen.
#elif [[ "$OSTYPE" == "freebsd"* ]]; then
        # ...
else
        # Unknown.
        echo "Only Linux and MacOS currently tested and supported!"
        exit
fi


NAME=$1


    echo """
Erase all
Select outer viewport: 0, 6.5, 0, 4
Read from file: \"$NAME\"
To Formant (burg): 0, 5, 5500, 0.025, 50
#Draw tracks: 0, 0, 5500, "yes"
List: \"yes\", \"no\", 3, \"no\", 2, \"no\", 2, \"yes\"
""" > temp.praat

    $PROGRAMNAME --run temp.praat > temp.txt
    rm temp.praat
    
    
    
