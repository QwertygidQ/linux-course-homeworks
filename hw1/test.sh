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

echo "[TEST]: Complex test"
echo "a Ivan Ivanov +75554433 ivan@ivanov.com 30" > /dev/phonebook_device
echo "a Petr Petrov +86665544 petr@petrov.ru 41" > /dev/phonebook_device
echo "a Alex Alexeev +97776655 alex@alexeev.ru 70" > /dev/phonebook_device

echo "f Petrov" > /dev/phonebook_device
cat /dev/phonebook_device

echo "f random" > /dev/phonebook_device
cat /dev/phonebook_device
echo `print_dmesg | grep failed`

echo "d Petrov" > /dev/phonebook_device
echo "f Petrov" > /dev/phonebook_device
cat /dev/phonebook_device
echo `print_dmesg | grep failed`

echo "a Petr Petrov +86665544 petr@petrov.ru 41" > /dev/phonebook_device
echo "f Ivanov" > /dev/phonebook_device
cat /dev/phonebook_device
echo "f Petrov" > /dev/phonebook_device
cat /dev/phonebook_device
echo "f Alexeev" > /dev/phonebook_device
cat /dev/phonebook_device

rmmod phonebook
echo "[TEST]: Removed the module"
