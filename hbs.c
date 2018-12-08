/*  hbs.c
 *
 *  Copyright 2018 Steven Anthony (Tony) Williams
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fnmatch.h>
#include <errno.h>

/* Masks and options determine how files are counted. */
/* If a MASK_ bit is set, that file type is skipped. */
#define  NO_OPTMASK         0x00000000    /* Skip nothing with no options */
#define  MASK_NONE_PO       0xFFFFFF80    /* Skip nothing preserving options */
#define  MASK_SYMLINK       0x00000001    /* Skip symbolic links */
#define  MASK_REGFILE       0x00000002    /* Skip regular files */
#define  MASK_DIR           0x00000004    /* Skip directories */
#define  MASK_CHARDEV       0x00000008    /* Skip character devices */
#define  MASK_BLKDEV        0x00000010    /* Skip block devices */
#define  MASK_FIFO          0x00000020    /* Skip fifo files */
#define  MASK_SOCKET        0x00000040    /* Skip sockets */
#define  MASK_ALL           0x0000007F    /* Skip all */
#define  MASK_DEFAULT       NO_OPTMASK    /* Default mask */

#define  OPT_NONE_PM        0x0000007F    /* No options preserving masks */
#define  OPT_FILTER         0x00000080    /* Count files matching pattern */
#define  OPT_RECURSIVE      0x00000100    /* Recursive to all sub directories */
#define  OPT_VERBOSE        0x00000200    /* Display files counted */
#define  OPT_TREE           0x00000400    /* Display files in a tree format */
#define  OPT_LINKS          0x00000800    /* Follow symbolic links */
#define  OPT_PERMISS        0x00001000    /* Show file permissions */
#define  OPT_USER           0x00002000    /* Show file's owner */
#define  OPT_GROUP          0x00004000    /* Show file's group */
#define  OPT_ORMODE         0x00008000    /* Show file matching any mode bits */
#define  OPT_ANDMODE        0x00010000    /* Show file matching all mode bits */
#define  OPT_XORMODE        0x00020000    /* Show file matching mode bits */
#define  OPT_COMMAND        0x00040000    /* Execute command on file(s) */
#define  OPT_EXTENSION      0x00080000    /* Extend command mode */
#define  OPT_BACKGRND       0x00100000    /* Execute command in background */
#define  OPT_DUMP           0x00200000    /* Dump the path (verbose without count) */
#define  OPT_DEFAULT        NO_OPTMASK    /* Default options */

#define  OPT_STRING         "ANLKREDYCVBOFISTbc:e:ghi:j:lpruvdtf:x:"
/* Remaining option letters */
/* GHJMPQUWXZ adkmnoqswyz */

#define  SetMask(a, m)      (a | m)
#define  ClearMask(a, m)    (a & ~m)
#define  SetOption(a, o)    (a | o)
#define  ClearOption(a, o)  (a & ~o)
#define  Is(a, m)           (a & m)
#define  IsNot(a, m)        ((a & m) ? false : true)
#define  ClearOptions(o)    (o & OPT_NONE_PM)
#define  ClearMasks(m)      (m & MASK_NONE_PO)
#define  CNT_MSG(a)         printf("Search path: %s\n", a)

#define  K                  (1024.0)
#define  MEG                (1024.0*1024.0)
#define  GIG                (MEG*1024.0)

#define  SMALL_BUF          80
#define  BIG_BUF            256

#ifndef __cplusplus
enum                        { false = 0, true };
typedef int                 bool;
#endif

typedef struct stat         STAT;
typedef unsigned long long  ul64;

extern int                  errno;

static struct option        opts[] =
{
   { "all_types", no_argument, 0, 'A' },
   { "no_types", no_argument, 0, 'N' },
   { "sym_link", no_argument, 0, 'L' },
   { "no_sym_link", no_argument, 0, 'K' },
   { "regular_file", no_argument, 0, 'R' },
   { "no_regular_file", no_argument, 0, 'E' },
   { "directory", no_argument, 0, 'D' },
   { "no_directory", no_argument, 0, 'Y' },
   { "char_device", no_argument, 0, 'C' },
   { "no_char_device", no_argument, 0, 'V' },
   { "block_device", no_argument, 0, 'B' },
   { "no_block_device", no_argument, 0, 'O' },
   { "fifo", no_argument, 0, 'F' },
   { "no_fifo", no_argument, 0, 'I' },
   { "socket", no_argument, 0, 'S' },
   { "no_socket", no_argument, 0, 'T' },
   { "filter", required_argument, 0, 'f' },
   { "or_mode", required_argument, 0, 'i' },
   { "and_mode", required_argument, 0, 'j' },
   { "xor_mode", required_argument, 0, 'x' },
   { "cmd_mode", required_argument, 0, 'c' },
   { "cmd_ext", required_argument, 0, 'e' },
   { "recursive", no_argument, 0, 'r' },
   { "verbose", no_argument, 0, 'v' },
   { "dump", no_argument, 0, 'd' },
   { "tree", no_argument, 0, 't' },
   { "follow_links", no_argument, 0, 'l' },
   { "permissions", no_argument, 0, 'p' },
   { "user_id", no_argument, 0, 'u' },
   { "group_id", no_argument, 0, 'g' },

   { "help", no_argument, 0, 'h' },
   { 0, 0, 0, 0 }
};

static short                permMask[9] =
{
   0400, 0200, 0100, 0040, 0020, 0010, 0004, 0002, 0001
};
static char                 perms[] = "rwxrwxrwx";
static char                 *help[] =
{
   "hbs: How Big Search utility ver: 1.0.0 by: Tony Williams (2018)\n",
   "usage: hbs [<options> <path> <masks> <options> <path>]\n",
   " Arguments:\n",
   "  One or more start 'path' arguments can be used. Each will be",
   "  searched separately and all will be added together. They can be",
   "  anywhere in the parameter list and in any order. If there are",
   "  none, the current working directory is assumed.\n",
   " Masks for types of files to count:\n",
   " -A, --all_types           Count all file types (default)",
   " -N, --no_types            Don't count any file types",
   " -L, --sym_link            Count symbolic links (as files: see -l option)",
   " -K, --no_sym_link         Don't count symbolic links",
   " -R, --regular_file        Count regular files",
   " -E, --no_regular_file     Don't count regular files",
   " -D, --directory           Count directories",
   " -Y, --no_directory        Don't count directories", 
   " -C, --char_device         Count character device files",
   " -V, --no_char_device      Don't count character device files",
   " -B, --block_device        Count block device files",
   " -O, --no_block_devices    Don't count block device files",
   " -F, --fifo                Count fifo files",
   " -I, --no_fifo             Don't count fifo files",
   " -S, --socket              Count socket files",
   " -T, --no_socket           Don't count socket files\n",
   " Other options:\n",
   " -fName, --filter=Name     Count files matching 'name' (wildcards ok)",
   " -jBits, --and_mode=Bits   Count files exactly matching octal mode 'bits'",
   " -iBits, --or_mode=Bits    Count files matching any octal mode 'bits'",
   " -xBits, --xor_mode=Bits   Count files matching octal mode 'bits'\n",
   " -cCmd,  --cmd_mode=Cmd    Execute the command on all files (Cmd file)",
   " -eExt,  --cmd_ext=Ext     Extension for cmd_mode (Cmd file.old file.Ext)",
   "                           Note: Cmd can use quotes (-c\"ls -l\").",
   "                           Also, any new files created from Cmd with -e",
   "                           will end up in the current directory.",
   " -h, --help                Show this display",
   " -l, --follow_links        Count (follow) link, instead of link size",
   "                           NOTE: There are no checks for recursive links.",
   " -r, --recursive           Recurse to subdirectores",
   " -v, --verbose             Display each file counted and/or Cmd executed",
   " -d, --dump                Dump the path (like verbose without count)\n",
   " The following options are ignored unless -v or --verbose are used.\n",
   " -p, --permissions         Show file's permissions",
   " -u, --user_id             Show file's user ID",
   " -g, --group_id            Show file's group ID",
   " -t, --tree                Display each file in a tree format\n",
   " Examples:\n",
   " Search the current directory for all '.c' files.\n",
   "       hbs -f*.c\n",
   " Search for all '.o' files starting at '/usr/local/src'. Recurse to all",
   " subdirectories and print all files counted.\n",
   "       hbs -rvf*.o /usr/local/src",
   " or:   hbs /usr/local/src -r -v -f*.o\n",
   " Search for all files that are not directories and be recursive starting",
   " at the current directory.\n",
   "       hbs -Y -r\n",
   " Search for all '.o' files starting at '~/local/src'. Recurse to all",
   " subdirectories and execute 'rm' on each file found.\n",
   "       hbs -rf*.o ~/local/src -crm\n",
   " Search for symbolic links only in '/usr/lib' and be verbose. The -N",
   " turns everything off and the -L turns on symbolic links. Notice it only",
   " counts the size of the link and not the destination. Add the -l option",
   " and notice that the file sizes are larger.\n",
   "       hbs -NL /usr/lib -v\n",
   ""
};

/*static double   byteCount = 0.0;*/
static ul64    byteCount = 0;
static ul64    optBits = OPT_DEFAULT | MASK_DEFAULT,
                fileCount = 0L,
                dirCount = 0L,
                modeBits = 0L;
static char     filePattern[SMALL_BUF],
                cmdString[SMALL_BUF],
                cmdExtString[SMALL_BUF],
                startPath[256];
static bool     isHelp = false,
                isFilter, isOr, isAnd, isXor,
                isRecursive = false,
                isVerbose = false,
                isDump = false,
                isPermissions = false,
                isUser = false,
                isGroup = false,
                isCmd = false,
                isExt = false,
                isBack = false,
                isTree = false;

void loadStatus(STAT *s, char *fileStat)
{
   register int   x = 0;

   /* Start out as NULL string */
   fileStat[0] = '\0';

   /* If displaying permissions... */
   if (isPermissions)
   {
      for (; x < 9; x++)
      {
         if (s->st_mode & permMask[x])
            fileStat[x] = perms[x];
         else
            fileStat[x] = '-';
      }
      fileStat[x] = '\0';
   }

   /* If displaying owner id... */
   if (isUser)
   {
      fileStat[x++] = ' ';
      sprintf(&fileStat[x], "uid:%d", s->st_uid);
      x = strlen(fileStat);
   }

   /* If displaying group id... */
   if (isGroup)
   {
      fileStat[x++] = ' ';
      sprintf(&fileStat[x], "gid:%d", s->st_gid);
   }
}

/* Caller must free output */
char *fix_filename(char *name)
{
   char   *out = (char*)malloc((strlen(name) * 2) + 1);;
   char   *ptr = out;

   while (*name != 0)
   {
      if ((*name == ' ') || (*name == '\t'))
      {
         *ptr++ = '\\';
         *ptr++ = *name++;
      }
      else if (*name == '\'')
      {
         *ptr++ = '\\';
         *ptr++ = *name++;
      }
      else if (*name == '(')
      {
         *ptr++ = '\\';
         *ptr++ = *name++;
      }
      else if (*name == ')')
      {
         *ptr++ = '\\';
         *ptr++ = *name++;
      }
      else if (*name == '&')
      {
         *ptr++ = '\\';
         *ptr++ = *name++;
      }
      else
         *ptr++ = *name++;
   }
   *ptr = 0;

   return out;
}

void search(char *path, int indent)
{
   DIR                  *dirPtr;
   struct dirent        *dirEntry;
   STAT                 statBuffer;
   /*unsigned long long   statSize;*/
   ul64                statSize;
   char                 endChar,
                        curPath[256],
                        linkPath[SMALL_BUF];
   bool                 countingFile,
                        isDir;

   if ((dirPtr = opendir(path)) == NULL)
      fprintf(stderr, "Could not open directory: %s [%s]\n", path, strerror(errno));
   else
   {
      char   fileStatus[SMALL_BUF];
      bool   patternMatch,
             orMatch, xorMatch,
             andMatch, isMatch;

      chdir(path);
      getcwd(curPath, 256);

      while((dirEntry = readdir(dirPtr)) != NULL)
      {
         lstat(dirEntry->d_name, &statBuffer);
         statSize = statBuffer.st_size;

         if (isFilter)
            patternMatch = (fnmatch(filePattern, dirEntry->d_name, 0)) ? false : true;
         else
            patternMatch = true;

         if (isOr)
            orMatch = (modeBits & (statBuffer.st_mode & 0777)) ? true : false;
         else
            orMatch = true;
            
         if (isXor)
         {
            ul64 bits = (statBuffer.st_mode & 0777);

            if ((modeBits & bits) && !(~modeBits & bits))
               xorMatch = true;
            else
               xorMatch = false;
         }  
         else
            xorMatch = true;
            
         if (isAnd)
         {
            ul64 bits = (statBuffer.st_mode & 0777);
            
            if ((modeBits == (modeBits & bits)) && !(~modeBits & bits))
               andMatch = true;
            else
               andMatch = false;
         }  
         else
            andMatch = true;

         isMatch = (patternMatch && orMatch && xorMatch && andMatch) ? true : false;
            
         linkPath[0] = '\0';
         endChar = ' ';
         isDir = false;

         if (S_ISDIR(statBuffer.st_mode))
         {
            /* Ignore . and .. */
            if (strcmp(".", dirEntry->d_name) == 0 ||
                strcmp("..", dirEntry->d_name) == 0)
               continue;

            isDir = true;

            if (isMatch && (countingFile = IsNot(optBits, MASK_DIR)))
            {
               endChar = '/';
               dirCount++;
            }
         }
         else if (S_ISLNK(statBuffer.st_mode))
         {
            countingFile = IsNot(optBits, MASK_SYMLINK);

            if (countingFile)
            {
               STAT   tempStat;
               bool   followLinks = Is(optBits, OPT_LINKS);

               stat(dirEntry->d_name, &tempStat);

               if (followLinks)
                  statSize = tempStat.st_size;

               /* See if it's a directory. */
               if (S_ISDIR(tempStat.st_mode))
               {
                  if (followLinks)
                     isDir = true;

                  endChar = '/';
                  dirCount++;
               }
/*
               sprintf(linkPath, " -> %s%c", dirEntry->d_name, endChar);
*/
            }
         }
         else if (S_ISREG(statBuffer.st_mode))
            countingFile = (isMatch && IsNot(optBits, MASK_REGFILE));
         else if (S_ISCHR(statBuffer.st_mode))
            countingFile = (isMatch && IsNot(optBits, MASK_CHARDEV));
         else if (S_ISBLK(statBuffer.st_mode))
            countingFile = (isMatch && IsNot(optBits, MASK_BLKDEV));
         else if (S_ISFIFO(statBuffer.st_mode))
            countingFile = (isMatch && IsNot(optBits, MASK_FIFO));
         else if (S_ISSOCK(statBuffer.st_mode))
            countingFile = (isMatch && IsNot(optBits, MASK_SOCKET));
         else
            countingFile = false;

         if (countingFile && isMatch)
         {
            /*byteCount += (double)statSize;*/
            byteCount += statSize;
            fileCount++;

            if (isVerbose | isDump)
            {
               loadStatus(&statBuffer, fileStatus);

               if (isTree)
               {
                  printf("%9Ld %s %*s%s%c\n", statSize, fileStatus,
                         indent, "", dirEntry->d_name, endChar);
               }
               else if (isDump)
               {
                  printf("%s/%s%c%s\n",
                         curPath, dirEntry->d_name, endChar, linkPath);
               }
               else
               {
                  printf("%9Ld %s %s/%s%c%s\n", statSize, fileStatus,
                         curPath, dirEntry->d_name, endChar, linkPath);
               }
            }

            if (isCmd)
            {
               int    nf;
               char   sysCmd[256],
                      newFile[SMALL_BUF],
                      extCmd[256];
               char   *ptr = dirEntry->d_name;       

               sysCmd[0] = 0;
               extCmd[0] = 0;

               for (nf = 0; (*(ptr+nf) != '.') && (nf < BIG_BUF); nf++)
                  newFile[nf] = *(ptr+nf);
               newFile[nf] = 0;

               if (isExt)
                  sprintf(extCmd, "%s/%s.%s", startPath, newFile, cmdExtString);

               /* Put '\' before each white space or special char */
               ptr = fix_filename(dirEntry->d_name);
               sprintf(sysCmd, "%s %s %s %s", cmdString,
                       ptr,
                       extCmd,
                       (isBack) ? "&" : "");
               free(ptr);

               if (isVerbose | isDump)
                  printf("Cmd: %s\n", sysCmd);

               system(sysCmd);
            }
         }

         if (isDir && isRecursive)
            search(dirEntry->d_name, indent+3);

         chdir(curPath);
      }
      chdir("..");
      closedir(dirPtr);
   }
}

void printHelp()
{
   register int   x;

   for (x = 0; *help[x]; x++)
      printf("%s\n", help[x]);

   exit(0);
}

int main(int argc, char *argv[])
{
   int   opt,
         optIndex;

   getcwd(startPath, 256);

   while ((opt = getopt_long(argc, argv, OPT_STRING, opts, &optIndex)) != -1)
   {
      switch (opt)
      {
         case 'A':
            optBits = ClearMask(optBits, MASK_ALL);
            break;
         case 'N':
            optBits = SetMask(optBits, MASK_ALL);
            break;
         case 'L':
            optBits = ClearMask(optBits, MASK_SYMLINK);
            break;
         case 'K':
            optBits = SetMask(optBits, MASK_SYMLINK);
            break;
         case 'R':
            optBits = ClearMask(optBits, MASK_REGFILE);
            break;
         case 'E':
            optBits = SetMask(optBits, MASK_REGFILE);
            break;
         case 'D':
            optBits = ClearMask(optBits, MASK_DIR);
            break;
         case 'Y':
            optBits = SetMask(optBits, MASK_DIR);
            break;
         case 'C':
            optBits = ClearMask(optBits, MASK_CHARDEV);
            break;
         case 'V':
            optBits = SetMask(optBits, MASK_CHARDEV);
            break;
         case 'B':
            optBits = ClearMask(optBits, MASK_BLKDEV);
            break;
         case 'O':
            optBits = SetMask(optBits, MASK_BLKDEV);
            break;
         case 'F':
            optBits = ClearMask(optBits, MASK_FIFO);
            break;
         case 'I':
            optBits = SetMask(optBits, MASK_FIFO);
            break;
         case 'S':
            optBits = ClearMask(optBits, MASK_SOCKET);
            break;
         case 'T':
            optBits = SetMask(optBits, MASK_SOCKET);
            break;

         case 'f':
            optBits = SetOption(optBits, OPT_FILTER);
            strcpy(filePattern, optarg);
            break;

         case 'p':
            optBits = SetOption(optBits, OPT_PERMISS);
            break;
         case 'i':
            optBits = SetOption(optBits, OPT_ORMODE);
            modeBits = atoi(optarg);
            sscanf(optarg, "%Lo", &modeBits);
            break;
         case 'j':
            optBits = SetOption(optBits, OPT_ANDMODE);
            modeBits = atoi(optarg);
            sscanf(optarg, "%Lo", &modeBits);
            break;
         case 'x':
            optBits = SetOption(optBits, OPT_XORMODE);
            modeBits = atoi(optarg);
            sscanf(optarg, "%Lo", &modeBits);
            break;
         case 'u':
            optBits = SetOption(optBits, OPT_USER);
            break;
         case 'g':
            optBits = SetOption(optBits, OPT_GROUP);
            break;
         case 'l':
            optBits = SetOption(optBits, OPT_LINKS);
            break;
         case 'r':
            optBits = SetOption(optBits, OPT_RECURSIVE);
            break;
         case 'v':
            optBits = SetOption(optBits, OPT_VERBOSE);
            break;
         case 'd':
            optBits = SetOption(optBits, OPT_DUMP);
            break;
         case 't':
            optBits = SetOption(optBits, OPT_TREE);
            break;
         case 'c':
            optBits = SetOption(optBits, OPT_COMMAND);
            strcpy(cmdString, optarg);
            break;
         case 'e':
            optBits = SetOption(optBits, OPT_EXTENSION);
            strcpy(cmdExtString, optarg);
            break;
         case 'b':
            optBits = SetOption(optBits, OPT_BACKGRND);
            break;
         case 'h':
            printHelp();
            break;

         case '?':
            isHelp = true;
            break;
         case ':':
            printf("-%c option needs a value.\n", opt);
            isHelp = true;
            break;
         default:
            break;
      }

      if (isHelp)
      {
         printf("Try --help or -h for a list of valid options.\n");
         break;     /* One is enough */
      }
   }

   if (!isHelp)
   {
      double   scaled_count = 0.0;
      char     scale_chr = 'G';

      isRecursive = Is(optBits, OPT_RECURSIVE);
      isVerbose = Is(optBits, OPT_VERBOSE);
      isDump = Is(optBits, OPT_DUMP);
      isTree = Is(optBits, OPT_TREE);
      isFilter = Is(optBits, OPT_FILTER);
      isOr = Is(optBits, OPT_ORMODE);
      isAnd = Is(optBits, OPT_ANDMODE);
      isXor = Is(optBits, OPT_XORMODE);
      isPermissions = Is(optBits, OPT_PERMISS);
      isUser = Is(optBits, OPT_USER);
      isGroup = Is(optBits, OPT_GROUP);
      isCmd = Is(optBits, OPT_COMMAND);
      isExt = Is(optBits, OPT_EXTENSION);
      isBack = Is(optBits, OPT_BACKGRND);

      if (isVerbose || isDump)
         printf("\n");

      /* If no start path argumnet... */
      if (optind == argc)
      {
         char   cwdBuf[BIG_BUF];

         if ((getcwd(cwdBuf, BIG_BUF)))
         {
            CNT_MSG(cwdBuf);
            search(cwdBuf, 0);
         }
         else
            printf("Could not read directory path: %s [%s]\n", cwdBuf, strerror(errno));
      }
      else
      {
         /* Search using all arguments as start paths. */
         for (; optind < argc; optind++)
         {
            CNT_MSG(argv[optind]);
            search(argv[optind], 0);
         }
      }

      if (byteCount < MEG)
      {
         scaled_count = (double)(byteCount / K);
         scale_chr = 'K';
      }
      else if (byteCount < GIG)
      {
         scaled_count = (double)(byteCount / MEG);
         scale_chr = 'M';
      }
      else
         scaled_count = (double)(byteCount / GIG);

      printf("\n%012Ld (%.1f%c) total bytes in %Ld file(s) (%Ld are directories)\n\n",
             byteCount, scaled_count, scale_chr, fileCount, dirCount);
/*
      printf("\n%09Ld total bytes in %Ld file(s) (%Ld are directories)\n\n",
             byteCount, fileCount, dirCount);
*/
   }

   exit(0);
}

/* hbs.c */
