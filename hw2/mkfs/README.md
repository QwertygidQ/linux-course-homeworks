# mkfs

Program for creating a filesystem file.

It is recommended to **not** change the default settings,
as there is no check for filesystems that do not have enough
blocks/inodes at the moment.

## Building
```
make
```

## Running
./mkfs FILE [BLOCK_SIZE TOTAL_BLOCKS TOTAL_INODES]
