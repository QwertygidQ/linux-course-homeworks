#!/bin/sh

print_dmesg()
{
    echo "[TEST]: Current dmesg output:"
    dmesg | tail | grep Phonebook | while read -r line; do
        echo $line
    done
}

make
insmod phonebook.ko

echo "[TEST]: Inserted the module"

print_dmesg

echo "[TEST]: Write test"
MSG="Hello, world!abcdefgh"
echo $MSG > /dev/phonebook_char
echo "[TEST] Tried to write '$MSG'"

print_dmesg

echo "[TEST]: Read test"
cat /dev/phonebook_char

print_dmesg

rmmod phonebook
echo "[TEST]: Removed the module"
