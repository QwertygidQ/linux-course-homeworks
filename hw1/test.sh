#!/bin/sh

# Rudimentary testing utility script, just sanity checks the module

print_dmesg()
{
    echo "[TEST]: Current dmesg output:"
    dmesg | tail -n 15 | grep Phonebook | while read -r line; do
        echo "  $line"
    done
}

make
insmod phonebook.ko

echo "[TEST]: Inserted the module"

print_dmesg

echo "[TEST]: Write/User creation test"
MSG="a Ivan Ivanov +75554433 ivan@ivanov.com 30"
echo $MSG > /dev/phonebook_device
echo "[TEST] Tried to write '$MSG'"

print_dmesg

echo "[TEST]: Read/User search test"
echo "f Ivanov" > /dev/phonebook_device
cat /dev/phonebook_device

print_dmesg

echo "[TEST]: User removal test"
echo "d Ivanov" > /dev/phonebook_device
echo "f Ivanov" > /dev/phonebook_device
cat /dev/phonebook_device

print_dmesg

rmmod phonebook
echo "[TEST]: Removed the module"
