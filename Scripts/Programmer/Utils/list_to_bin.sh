#!/bin/bash

# TODO: Rewrite this script, and add features like path arguments, error handling, etc.

printf "\nConverting code.list to code.bin\n"
#create code.bin from code.list (comments can stay after lines)
    perl -ne 'print pack("B32", $_)' < code.list > ../UART/code.bin

#printf "to verify, use: \nxxd -b -c4 code.bin\n"
#printf "to create intel hex file for quartus, use: \nsrec_cat code.bin -binary -output -intel > code.hex\n"

#add padding unless specified to not add padding
if [[ $1 != "noPadding" ]]; then

    printf "Padding code.bin with ones until multiple of 4096\n"

    #get file size
        SIZE=$(wc -c < code.bin)

    printf "Size of code.bin is $SIZE bytes\n"

    #get needed bytes to make multiple of 4096
        NEEDED=$((4096-($SIZE%4096)))

    printf "Need $NEEDED more bytes for next multiple of 4096\n"

    printf "Appending $NEEDED bytes to code.bin\n"

        dd if=<(yes $'\xFF' | tr -d "\n") bs=1 count=$NEEDED >> code.bin

fi

FILESIZE=`du -hs code.bin | cut -f1`
echo "Size of binary: $FILESIZE"

printf "Done compiling binary files\n"
