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
Usage: dosfs --<subcmd> <options> <..args..>
Usage: dos<subcmd> <options> <..args..>
Valid subcommands are: dir read write mkdir del move attrib format
Common options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
```

```
Usage: dosdir <options> <path[/pattern]>
       dosfs --dir <options>  <path[/attern]>
List the contents of a directory <path> matching the optional
pattern <pattern>. Option -b selects a compact output suitable
for shell scripts. Otherwise this command outputs information
with a format simular to the well known `dir` dos command.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-b            :  only display the full path of each file, one per line
	-s            :  recursively display files in subdirectories
	-x            :  display short file names when they're different
```

```
Usage: dosread <options> {<path>}}
       dosfs --read {<path>}
Read the files specified by <path> and copy them to stdout.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-o <outfile>  :  copy to <outfile> instead of stdout.
```

```
Usage: doswrite <options> <path>
       dosfs --write <options> <path>
Write stdin to the specified <path>.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-i <infile>   :  writes <infile> instead of stdin.
	-a            :  append to the possibly existing file <path>.
	-d            :  create missing directories
	-q            :  overwrite existing files
```

```
Usage: dosmkdir <options> <path>}}
       dosfs --mkdir <options> <path>}}
Create a subdirectory named <path>.
This command fails if {path} already exists or if its parent
directory does not exists. In contrast, with option -q, this
command creates all the necessary subdirectories.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-q            :  create all necessary subdirs
```

```
Usage: dosdel <options> {<path>[/<pattern>]}
       dosfs --del <options> {<path>[/<pattern>]}
Delete files or subtrees named <path> or matching <path>/<pattern>.
By default this command prompts before deleting subdirectories or files
matching a pattern. Use options -i or -q to prompt more or not at all.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-i            :  always prompt before deleting
	-q            :  delete files and trees without prompting
```

```
Usage: dosmove <options> {<src>} <dest>}}
       dosfs --move  <options> {<src>} <dest>}}
Move or rename files or subtrees.
When this command is called with a single source path <src>, the
destination <dest> can be an existing directory or give a new filename
inside an existing directory. When multiple source paths are given
the destination <dest> must be an existing directory.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-q            :  overwrite files without prompting
```

```
Usage: dosattrib <options> [<pattern>]
       dosfs --attrib <options> [<pattern>]
Display or change file attributes.
When used with options [+|-][ARHS], this command changes the
attributes of the selected file. When used without options
this command prints the attributes.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	+A -A         :  set or remove the archive bit.
	+R -R         :  set or remove the read-only bit.
	+H -H         :  set or remove the hidden bit.
	+S -S         :  set or remove the system bit.
	-s            :  process directories recursively.
	-d            :  change directory attributes.
```

```
Usage: dosformat <options> [<label>]
       dosfs --format <options> [<label>]
Format a filesystem.
With option -p, this command expects a partionned drive and
formats the specified partition. Otherwise it formats the entire
disk or disk image with a filesystem with or without a partition table.
Options:
	-h            :  show more help
	-f <filename> :  specify a device or image file (required).
	-p <partno>   :  specify a partition number (1..4)
	-s            :  create a filesystem without a partition table.
	-F <fs>       :  specify a filesystem: FAT, FAT32, or EXFAT.
```
