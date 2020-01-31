# Phonebook Linux kernel module

## Building
```
make
```

## Running
```
sudo insmod phonebook.ko
```

## Disabling
```
sudo rmmod phonebook
```

## Testing
```
sudo sh test.sh
```

## Usage
Every word should be separated by **exactly** one space character.

Add user:
```
sudo sh -c "echo 'a *name* *surname* *phone* *email* *age (number)*' > /dev/phonebook_device"
```

Delete user:
```
sudo sh -c "echo 'd *surname*' > /dev/phonebook_device"
```

Find user (and read info about them):
```
sudo sh -c "echo 'f *surname*' > /dev/phonebook_device"
sudo cat /dev/phonebook_device
```

Reading logs:
```
sudo dmesg | tail | grep Phonebook
```
