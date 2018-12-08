How Big Search (hbs)

A command line utility that searches for files, and tells you
how big it all is.

I created the first version of this utility way back in the mid 1990's
on Slackware Linux. I didn't know how to use 'find' back then. I needed
a way to search for files, and get the size of folders. I use it all the
time, and it hasn't changed much since its creation. It has a lot of
options - most of which I don't even use, so there may be bugs I've never
encountered.

To build the program:

$ make

You'll need 'gcc' & 'make' to do the build.

For Ubuntu (18.04)
$ sudo apt install gcc make

For older versions
$ sudo apt-get install gcc make

If you don't want to install 'make', this will also work:

$ gcc hbs.c -o hbs

It should build properly on older 32 bit machines. It works on a Dell
Optiplex GX270 running Ubuntu 16.04.5 LTS (32 bit).

There is no man page, and I have not plans to create one. Others are welcome
to do that, of course.

The following is a dump of the options help:

$ hbs -h
hbs: How Big Search utility ver: 1.0.0 by: Tony Williams (2018)

usage: hbs [<options> <path> <masks> <options> <path>]

 Arguments:

  One or more start 'path' arguments can be used. Each will be
  searched separately and all will be added together. They can be
  anywhere in the parameter list and in any order. If there are
  none, the current working directory is assumed.

 Masks for types of files to count:

 -A, --all_types           Count all file types (default)
 -N, --no_types            Don't count any file types
 -L, --sym_link            Count symbolic links (as files: see -l option)
 -K, --no_sym_link         Don't count symbolic links
 -R, --regular_file        Count regular files
 -E, --no_regular_file     Don't count regular files
 -D, --directory           Count directories
 -Y, --no_directory        Don't count directories
 -C, --char_device         Count character device files
 -V, --no_char_device      Don't count character device files
 -B, --block_device        Count block device files
 -O, --no_block_devices    Don't count block device files
 -F, --fifo                Count fifo files
 -I, --no_fifo             Don't count fifo files
 -S, --socket              Count socket files
 -T, --no_socket           Don't count socket files

 Other options:

 -fName, --filter=Name     Count files matching 'name' (wildcards ok)
 -jBits, --and_mode=Bits   Count files exactly matching octal mode 'bits'
 -iBits, --or_mode=Bits    Count files matching any octal mode 'bits'
 -xBits, --xor_mode=Bits   Count files matching octal mode 'bits'

 -cCmd,  --cmd_mode=Cmd    Execute the command on all files (Cmd file)
 -eExt,  --cmd_ext=Ext     Extension for cmd_mode (Cmd file.old file.Ext)
                           Note: Cmd can use quotes (-c"ls -l").
                           Also, any new files created from Cmd with -e
                           will end up in the current directory.
 -h, --help                Show this display
 -l, --follow_links        Count (follow) link, instead of link size
                           NOTE: There are no checks for recursive links.
 -r, --recursive           Recurse to subdirectores
 -v, --verbose             Display each file counted and/or Cmd executed
 -d, --dump                Dump the path (like verbose without count)

 The following options are ignored unless -v or --verbose are used.

 -p, --permissions         Show file's permissions
 -u, --user_id             Show file's user ID
 -g, --group_id            Show file's group ID
 -t, --tree                Display each file in a tree format

 Examples:

 Search the current directory for all '.c' files.

       hbs -f*.c

 Search for all '.o' files starting at '/usr/local/src'. Recurse to all
 subdirectories and print all files counted.

       hbs -rvf*.o /usr/local/src
 or:   hbs /usr/local/src -r -v -f*.o

 Search for all files that are not directories and be recursive starting
 at the current directory.

       hbs -Y -r

 Search for all '.o' files starting at '~/local/src'. Recurse to all
 subdirectories and execute 'rm' on each file found.

       hbs -rf*.o ~/local/src -crm

 Search for symbolic links only in '/usr/lib' and be verbose. The -N
 turns everything off and the -L turns on symbolic links. Notice it only
 counts the size of the link and not the destination. Add the -l option
 and notice that the file sizes are larger.

       hbs -NL /usr/lib -v

