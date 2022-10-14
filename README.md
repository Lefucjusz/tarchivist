
# tarchivist
A small, simple tar library implementation based on rxi's [microtar](https://github.com/rxi/microtar).  
The library consists of only two files - `tarchivist.c` and `tarchivist.h` - and is written in ANSI C, what makes it easy to integrate into a wide variety of projects.

## Functionalities
* Creating, writing and appending files to the tar archives
* Reading file information from the tar archives
* Reading file contents from the tar archives
* Searching for the file with a given name in the tar archive
* POSIX.1-1988 (*UStar*) tar header compliance
* Proper archive finalizing mechanism
* Custom stream interface

## Examples
The following examples presenting the usage of the library have been included:
* *read_demo* - presents the functionalities of reading the tar file;
* *write_demo* - presents the functionalities of creating and writing the tar file;
* *packer* - implements very simple *tar*-like utility that can perform packing and unpacking of an archive;
* *packer-custom-stream* - presents how to use custom stream interface; apart from that has the same functionality as packer.

### Running the examples
The examples use *POSIX* calls and libraries, so they have to be compiled under the environment that supports them.
#### Clone the repo
```shell
git clone https://github.com/Lefucjusz/tarchivist
```
or:
```shell
git clone git@github.com:Lefucjusz/tarchivist.git
```
#### Build and run the desired example
```shell
cd tarchivist
```
##### Build all
```shell
make
```
##### Build and run *read-demo*
```shell
make read-demo
cd build/bin
./read-demo
```
##### Build and run *write-demo*
```shell
make write-demo
cd build/bin
./write-demo
```
##### Build *packer*
```shell
make packer
```
##### Run *packer* in pack mode
`````shell
cd build/bin
./packer -p -s some_folder -d archive_to_pack_the_folder_to.tar
`````

##### Run *packer* in unpack mode
`````shell
cd build/bin
./packer -u -s some_archive.tar -d folder_to_unpack_the_archive_to
`````

##### Build *packer*'s debug version (with *-Og* and *-ggdb3* flags) 
```shell
make packer-debug
```

##### Build *packer-custom-stream*
```shell
make packer-custom-stream
```

## Custom stream interface
By default, the library reads and writes to a standard file using `stdio` file handling functions. It is, however, possible to initialize the `tarchivist_t` struct with custom stream callbacks and stream pointer to operate on something different than a file.
#### Callbacks that have to be provided to read an archive from a stream
* `int seek(tarchivist_t *tar, long offset, int whence) - sets the position of the stream cursor`
* `long tell(tarchivist_t *tar) - gets the current position of the stream cursor`
* `int read(tarchivist_t *tar, unsigned size, void *data) - reads 'size' bytes from the stream into the 'data'`
* `int close(tarchivist_t *tar) - closes the stream`

#### Callbacks that have to be provided to write an archive to a stream
* `int seek(tarchivist_t *tar, long offset, int whence) - sets the position of the stream cursor`
* `long tell(tarchivist_t *tar) - gets the current position of the stream cursor`
* `int read(tarchivist_t *tar, unsigned size, void *data) - reads 'size' bytes from the stream into the 'data'`
* `int write(tarchivist_t *tar, unsigned size, const void *data) - writes 'size' bytes from the 'data' to the stream`
* `int close(tarchivist_t *tar) - closes the stream`

All callbacks should return `TARCHIVIST_SUCCESS` on success and negative return code on failure, except for `tell`, which should return current position of stream cursor on success and negative return code on failure.

When operating the library with a custom stream, the `tarchivist_open` function shall not be used. The stream shall be opened manually and all unused `tarchivist_t` struct fields shall be zero-filled.

## Things to improve

### Closing record detection
The algorithm detecting whether an archive is finalized (i.e. contains two null records at the end) is very straightforward. Only two conditions are checked:
* if the size of an archive file is at least 1024 bytes (the size of two null records);
* if the last 1024 bytes are all zeros.

When both of these are true, the archive is assumed to be finalized. This creates at least one unhandled corner case that I'm aware of - if the archive is not finalized, but the last file in the archive contains at least 1024 zero bytes, the algorithm will treat it as if it's finalized. This will lead to data corruption while appending new files to an existing tar, as those last 1024 bytes will be overwritten. 

Such a case seemed so unlikely to me that I decided not to change the algorithm, but if someone would like to fix it, one of the solutions that came to my mind is to:
* get the size of the last file;
* check if there's another 1024 bytes after the file's content;
* check if those 1024 bytes are all zeros.

This solution would be much more robust, but also more time complex, especially when dealing with archives that contain a lot of files, as it requires finding the last file in the archive.

## Credits
tarchivist is based on rxi's [microtar](https://github.com/rxi/microtar).

## License
This library is free software; you can redistribute it and/or modify it under the terms of the MIT license. See [LICENSE](https://github.com/Lefucjusz/tarchivist/blob/main/LICENSE) for details.
