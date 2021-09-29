# dosfs
Simple command line tool to manage a fat disk image (based on fatfs)

This project was written very quickly because I wanted something more convenient than mouting a disk image as root and less complicated than the venerable mtools. The compact executable `dosfs` implements several subcommands that can be either selected with argument, e.g. `--dir` or `--read`, or preselected by invoking it through a symbolic link whose name contains the command name, e.g. `dosdir`, `dosread`, etc.

Three options are recognized by all subcommands:
* `-f <imagefile>` specify the device or the image file
* `-p <partno>` specify a partition number. This only works when the file contains a partition table. Without this option the program either searches a file system without partition table, or selects the first partition of the table.
* `-h` provides help on the selected subcommand.
 
Building this program on Unix is just a matter of compiling it with `make` and installing it
with `make install bindir=/where/you/want/bin`. I haven't yet tried to compile it under Windows
but it should be close to work.
 
The following lists the help associated with each subcommand:

```
Usage: dosdir <options> <path[/pattern]>
       dosfs --dir <options>  <path[/attern]>
List the contents of a directory <path> matching the optional pattern <pattern>
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-a            :  display all files including system and hidden ones
	-b            :  only display the full path of each file, one per line
	-x            :  dispaly short file names when they're different
```

```
Usage: dosread {<paths>}}
       dosfs --read {<paths>}
Copy the specified {<paths>} to stdout
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
```

```
Usage: doswrite <options> <path>}}
       dosfs --write <options> <path>}}
Writes stdin to the specified {<path>}.
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-a            :  appends to the possibly existing file <path>.
	-q            :  silently overwrites an existing file
```

```
Usage: dosmkdir <options> <path>}}
       dosfs --mkdir <options> <path>}}
Creates a subdirectory named {<path>}.
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-q            :  silently create all subdirs
```

```
Usage: dosdel <options> {<paths>}}}
       dosfs --del <options> {<paths>}}}
Delete files or subtrees named <paths>.
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-i            :  always prompt before deleting
	-q            :  silently deletes files without prompting
```

```
Usage: dosmove <options> {<srcpaths>} <destpath|destdir>}}
       dosfs --move  <options> {<srcpaths>} <destpath|destdir>}}
Move or rename files or subtrees.
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-q            :  silently overwrites files without prompting
```

```
Usage: dosformat <options> [<label>]
       dosfs --format <options> [<label>]
Format a filesystem.
With option -p, this command expects a partionned drive and
formats the specified partition. Otherwise it formats the entire
disk or disk image with a filesystem with or without a partition table.
Options:
	-f <filename> :  specifies a device or image file
	-p <partno>   :  specifies a partition number (1..4)
	-s            :  creates a filesystem without a partition table.
	-F <fs>       :  specifies a filesystem: FAT, FAT32, or EXFAT.
```
