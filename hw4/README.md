# PS/2 keyboard interrupt handler module

Prints how many keys were pressed in the last minute to the kernel message buffer.

## Building
```
make
```

## Running
```
sudo insmod keyboard_stats.ko
```

## Disabling
```
sudo rmmod keyboard_stats
```

## Reading logs
```
sudo dmesg | tail | grep "Keyboard stats module"
```
