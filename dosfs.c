/*----------------------------------------------------------------------------/
/  DosFs - Command line tools to manage FAT disk images                       /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2021, lb3361, all right reserved.
/
/ This command line tool is an open source software. Redistribution and use in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/
/----------------------------------------------------------------------------*/


#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <locale.h>
#include <stdarg.h>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
#else
# error "TBD"
#endif

#include "ff.h"
#include "diskio.h"




/* -------------------------------------------- */
/*   MESSAGES                                   */
/* -------------------------------------------- */

void fatal(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr,"dosfs: ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

void warning(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr,"dosfs warning: ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

void fatal_code(FRESULT code)
{
  switch(code)
    {
    case FR_OK: break;
    case FR_NO_FILESYSTEM: fatal("Cannot find fat or exfat filesystem\n");
    case FR_DISK_ERR: fatal("I/O error\n");
    case FR_NO_FILE: fatal("File not found\n");
    case FR_NO_PATH: fatal("Path not found\n");
    case FR_INVALID_NAME: fatal("Invalid file name\n");
    case FR_DENIED: fatal("Permission denied\n");
    case FR_WRITE_PROTECTED: fatal("Write protected\n");
    case FR_INVALID_PARAMETER: fatal("Invalid parameter\n");
    case FR_EXIST: fatal("File already exists\n");
    case FR_MKFS_ABORTED: fatal("Formatting failed\n");
    default: fatal("Internal error %d\n", (int)code);
    }
  exit(EXIT_FAILURE);
}
  
void common_options(void)
{
  fprintf(stderr,
          "\t-h            :  show more help\n"
          "\t-f <filename> :  specify a device or image file (required).\n"
          "\t-p <partno>   :  specify a partition number (1..4)\n" );
}

int prompt(const char *fmt, ...)
{
  int res = -1;
  char buffer[16];
  char *s;

  for(;;) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, " (Y/N)? ");
    fflush(stdin);
    fflush(stderr);
    if (! (s = fgets(buffer, sizeof(buffer), stdin)))
      return 0;
    while (*s == '\t' || *s == ' ')
      s++;
    if (! strncasecmp(s, "yes", 3)) {
      res = 1; s += 3;
    } else if (! strncasecmp(s, "no", 2)) {
      res = 0; s += 2;
    } else if (! strncasecmp(s, "y", 1)) {
      res = 1; s += 1;
    } else if (! strncasecmp(s, "n", 1)) {
      res = 0; s += 1;
    } 
    while (*s == '\t' || *s == ' ')
      s++;
    if (*s == '\n')
      return res;
    for(; *s; s++)
      if ((res = *s) == '\n')
        break;;
    while (res != '\n' && res != EOF)
      res = getchar();
    if (res == EOF)
      break;
  }
  return 0;
}




/* -------------------------------------------- */
/* FATFS SUPPORT                                */
/* -------------------------------------------- */

#if FF_MULTI_PARTITION
PARTITION VolToPart[FF_VOLUMES];
#endif

#if FF_USE_LFN == 3
void* ff_memalloc (UINT msize) { return malloc(msize); }
void ff_memfree (void* mblock) { free(mblock); }
#endif

FATFS vol;

/* -------------------------------------------- */
/* FATFS DISKIO                                 */
/* -------------------------------------------- */

const char *fn = 0;
const char *sfn = 0;
int fd = -1;
int wp = 0;

DSTATUS disk_status (BYTE pdrv)
{
  (void) pdrv;
  if (fd < 0)
    return STA_NOINIT;
  if (wp)
    return STA_PROTECT;
  return 0;
}

DSTATUS disk_initialize (BYTE pdrv)
{
  if ((fd = open(fn, O_RDWR)) < 0) {
    wp = 1;
    if ((fd = open(fn, O_RDONLY)) < 0) {
      fatal("Cannot open file \"%s\"\n", fn);
      return STA_NOINIT;
    }
  }
  return disk_status(pdrv);
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector,	UINT count)
{
  (void) pdrv;
  size_t sz = (size_t)count * 512;
  ssize_t rsz;

  if (! fd)
    return RES_NOTRDY;
  if (lseek(fd, (off_t)(sector * 512), SEEK_SET)  == (off_t)(-1))
    return RES_PARERR;
  while (sz > 0) {
    rsz = read(fd, buff, sz);
    if (rsz < 0)
      return RES_ERROR;
    if (rsz == 0)
      return RES_PARERR;
    sz -= rsz;
    buff += rsz;
  }
  return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
  (void) pdrv;
  size_t sz = (size_t)count * 512;
  ssize_t rsz;

  if (! fd)
    return RES_NOTRDY;
  if (wp)
    return RES_WRPRT;
  if (lseek(fd, (off_t)(sector * 512), SEEK_SET) == (off_t)(-1))
    return RES_PARERR;
  while (sz > 0) {
    rsz = write(fd, buff, sz);
    if (rsz < 0)
      return RES_ERROR;
    if (rsz == 0)
      return RES_PARERR;
    sz -= rsz;
    buff += rsz;
  }
  return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
  (void) pdrv;
  switch(cmd)
    {
    case CTRL_TRIM:
    case CTRL_SYNC:
      {
        return RES_OK;
      }
    case GET_SECTOR_SIZE:
      {
        *(WORD*)buff = 512;
        return RES_OK;
      }
    case GET_SECTOR_COUNT:
      {
        off_t sz;
        if (fd < 0)
          return RES_NOTRDY;
        sz = lseek(fd, 0, SEEK_END);
        if (sz == (off_t)-1)
          return RES_ERROR;
        *(LBA_t*)buff = (LBA_t)sz / 512;
        return RES_OK;
      }
    default:
      {
        break;
      }
    }
  return RES_PARERR;
}
 
DWORD get_fattime (void)
{
  DWORD res = 0;
  time_t tim = time(NULL);
  struct tm *tm = localtime(&tim);

  res |= ((tm->tm_year - 80) & 0x7f ) << 25;
  res |= ((tm->tm_mon + 1) & 0xf) << 21;
  res |= ((tm->tm_mday) & 0x1f) << 16;
  res |= ((tm->tm_hour) & 0x1f) << 11;
  res |= ((tm->tm_min) & 0x3f) << 5;
  res |= ((tm->tm_sec) & 0x3f) >> 1;
  return res;
}
 

/* -------------------------------------------- */
/* UTILITIES                                    */
/* -------------------------------------------- */


char *strconcat(const char *str1, ...)
{
  va_list ap;
  const char *s;
  char *res, *d;
  int l = strlen(str1);

  va_start(ap, str1);
  while ((s = va_arg(ap, const char*)))
    l += strlen(s);
  va_end(ap);
  if (! (res = malloc(l + 1)))
    fatal("out of memory\n");
  strcpy(res, str1);
  d = res + strlen(res);
  va_start(ap, str1);
  while ((s = va_arg(ap, const char*))) {
    strcpy(d, s);
    d += strlen(d);
  }
  va_end(ap);
  return res;
}

char *fix_path(const char *path)
{
  char *s, *np;

  while (*path == '/' || *path == '\\')
    path += 1;
  if (! (np = strdup(path)))
    fatal("out of memory\n");
  for (s = np; *s; s++)
    if (*s == '\\')
      *s = '/';
  while (s > np && s[-1] == '/')
    *--s = 0;
  return np;
}


int pattern_p(const char *path)
{
  if (strpbrk(path, "*?"))
    return 1;
  return 0;
}

int file_p(const char *path)
{
  FILINFO info;
  if (f_stat(path, &info) == FR_OK)
    if (! (info.fattrib & AM_DIR))
      return 1;
  return 0;
}

int dir_p(const char *path)
{
  FILINFO info;

  if (path[0] == 0 || path[0] == '/' && path[1] == 0)
    return 1;
  if (f_stat(path, &info) == FR_OK)
    if (info.fattrib & AM_DIR)
      return 1;
  return 0;
}

FRESULT rmkdir(char *path, int qflag)
{
  if (qflag) {
    char *s = strrchr(path,'/');
    if (dir_p(path))
      return FR_OK;
    if (s) {
      *s = 0;
      rmkdir(path, qflag);
      *s = '/';
    }
  }
  return f_mkdir(path);
}

const char *format_date(WORD date)
{
  static char buffer[12];

  sprintf(buffer, "%02d/%02d/%04d",
          (date >> 5) & 0xf, (date & 0x1f), 1980 + ((date >> 9) & 0x3f) );
  return buffer;
}

const char *format_time(WORD time)
{
  static char buffer[12];
  int hour = (time >> 11) & 0x1f;
  const char *ampm = (hour >= 12) ? "PM" : "AM";

  if (hour >= 12)
    hour -= 12;
  if (hour == 0)
    hour = 12;
  sprintf(buffer, "%02d:%02d %2s",
          hour, (time >> 5) & 0x3f, ampm);
  return buffer;
}

void print_filinfo(FILINFO *inf, int xflag)
{
  printf("%s %s %8s %8lld ",
         format_date(inf->fdate), format_time(inf->ftime),
         (inf->fattrib & AM_DIR) ? "<DIR>" : "",
         (long long)(inf->fsize) );
#if FF_USE_LFN
  if (xflag)
    printf("%-12s ",
           (inf->altname[0] && strcasecmp(inf->fname, inf->altname))
           ? inf->altname : "");
  printf("%s\n", inf->fname);
#else
  printf("%s\n", inf->fname);
#endif
}


/* -------------------------------------------- */
/* DOSDIR                                       */
/* -------------------------------------------- */

void dosdirhelp(void)
{
  fprintf(stderr,
          "Usage: dosdir <options> <path[/pattern]>\n"
          "       dosfs --dir <options>  <path[/attern]>\n"
          "List the contents of a directory <path> matching the optional\n"
          "pattern <pattern>. Option -b selects a compact output suitable\n"
          "for shell scripts. Otherwise this command outputs information\n"
          "with a format simular to the well known `dir` dos command.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-b            :  only display the full path of each file, one per line\n"
          "\t-s            :  recursively display files in subdirectories\n"
          "\t-x            :  display short file names when they're different\n" );
}

FRESULT rdir(char *path, char *pattern, int sflag, int bflag, int xflag,
             int *pnfiles, FSIZE_t *psfiles, int *pndirs)
{
  int ndirs;
  DIR dir;
  FILINFO info;
  FRESULT res;

  while (path[0] == '/')
    path += 1;
  if (! bflag)
    printf("\n Directory of [%s]:/%s\n\n", sfn, path);
  res = f_findfirst(&dir, &info, path, pattern);
  if (res != FR_OK && res != FR_NO_FILE)
    return res;
  while (res == FR_OK && info.fname[0])
    {
      if (info.fattrib & AM_DIR) {
        *pndirs += 1;
      } else {
        *pnfiles += 1;
        *psfiles += info.fsize;
      }
      if (bflag) {
        if (! (info.fattrib & AM_DIR))
          printf("%s%s/%s\n", path[0] ? "/" : "", path, info.fname);
      } else {
        print_filinfo(&info, xflag);
      }
      res = f_findnext(&dir, &info);
    }
  f_closedir(&dir);
  if (sflag) {
    res = f_findfirst(&dir, &info, path, "*");
    if (res != FR_OK && res != FR_NO_FILE)
      return res;
    while (res == FR_OK && info.fname[0]) {
      if (info.fattrib & AM_DIR) {
        char *npath = strconcat(path, "/", info.fname, 0);
        rdir(npath, pattern, sflag, bflag, xflag, pnfiles, psfiles, pndirs);
        free(npath);
      }
      res = f_findnext(&dir, &info);
    }
  }
  f_closedir(&dir);
  return res;
}

FRESULT dosdir(int argc, const char **argv)
{
  int i;
  int bflag = 0;
  int sflag = 0;
  int xflag = 0;
  char *path  = "";
  char *pattern = 0;
  char label[40];
  DWORD serial;
  FRESULT res;
  int ndirs = 0;
  int nfiles = 0;
  FSIZE_t sfiles = 0;
  DWORD ncls;
  FATFS *vol;

  for (i=1; i<argc; i++) {
    if (! strcmp(argv[i], "-b"))
      bflag = 1;
    else if (! strcmp(argv[i], "-s"))
      sflag = 1;
    else if (! strcmp(argv[i], "-x"))
      xflag = 1;
    else if (argv[i][0] == '-')
      goto usage;
    else if (!path || !path[0])
      path = fix_path(argv[i]);
    else {
    usage:
      dosdirhelp();
      exit(EXIT_FAILURE);
    }
  }
  pattern = (pattern) ? pattern + 1 : path;
  if (dir_p(path)) {
    pattern = "*";
  } else if ((pattern = strrchr(path, '/'))) {
    *pattern = 0;
    pattern += 1;
  } else {
    pattern = path;
    path = "";
  }
  if (! bflag && f_getlabel("", label, &serial) == FR_OK) {
    if (label[0])
      printf(" Volume label: %s\n", label);
    else
      printf(" Volume has no label\n");
    printf(" Volume Serial Number is %04X-%04X\n", (serial >> 16) & 0xffff, serial & 0xffff);
  }
  res = rdir(path, pattern, sflag, bflag, xflag, &nfiles, &sfiles, &ndirs);
  if (res == FR_OK && ! bflag) {
    if (nfiles + ndirs == 0)
      printf("File not found\n");
    printf("\n");
    f_getfree(path, &ncls, &vol);
    printf("    %8d File(s) %12lld bytes\n", nfiles, (long long)sfiles);
    printf("    %8d Dir(s)  %12lld bytes free\n", ndirs, (long long)ncls * vol->csize * 512);
  }
  return res;
}


/* -------------------------------------------- */
/* DOSREAD                                      */
/* -------------------------------------------- */

void dosreadhelp(void)
{
  fprintf(stderr,
          "Usage: dosread <options> {<path>}}\n"
          "       dosfs --read {<path>}\n"
          "Read the files specified by <path> and copy them to stdout.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-o <outfile>  :  copy to <outfile> instead of stdout.\n");
}

FRESULT dosread(int argc, const char **argv)
{
  int i;
  FRESULT res;
  
  if (argc < 2) {
  usage:
    dosreadhelp();
    exit(EXIT_FAILURE);
  }
#ifdef WIN32
  fflush(stdout);
  _setmode(_fileno(stdout), O_BINARY);
#endif
  for (i=1; i<argc; i++)
    {
      if (! strcmp(argv[i], "-o")) {
        if (++i >= argc)
          goto usage;
        if (! freopen(argv[i], "wb", stdout))
          fatal("Cannot open '%s' for writing\n", argv[i]);
      } else if (argv[i][0] == '-') {
        goto usage;
      } else {
        FIL fil;
        char *path = fix_path(argv[i]);
        char buffer[4096];
        UINT nread;
        res = f_open(&fil, path, FA_READ);
        if (res != FR_OK)
          return res;
        do {
          res = f_read(&fil, buffer, (UINT)sizeof(buffer), &nread);
          if (res != FR_OK)
            break;
          if (nread > 0)
            fwrite(buffer, 1, nread, stdout);
        } while (nread == (UINT)sizeof(buffer));
        fflush(stdout);
        f_close(&fil);
        if (res != FR_OK)
          return res;
      }
    }
  return FR_OK;
}


/* -------------------------------------------- */
/* DOSWRITE                                     */
/* -------------------------------------------- */

void doswritehelp(void)
{
  fprintf(stderr,
          "Usage: doswrite <options> <path>\n"
          "       dosfs --write <options> <path>\n"
          "Write stdin to the specified <path>.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-i <infile>   :  writes <infile> instead of stdin.\n"
          "\t-a            :  append to the possibly existing file <path>.\n"
          "\t-d            :  create missing directories\n"
          "\t-q            :  overwrite existing files\n");
}

FRESULT doswrite(int argc, const char **argv)
{
  int i;
  char *s;
  char *path = 0;
  BYTE mode = FA_WRITE | FA_CREATE_NEW;
  FRESULT res;
  FIL fil;
  char buffer[4096];
  UINT nread, nwritten;
  int dflag = 0;

  for (i=1; i<argc; i++) {
    if (! strcmp(argv[i],"-a"))
      mode = FA_WRITE | FA_OPEN_APPEND;
    else if (! strcmp(argv[i],"-q"))
      mode = FA_WRITE | FA_CREATE_ALWAYS;
    else if (! strcmp(argv[i],"-i")) {
      if (++i >= argc)
        goto usage;
      if (! freopen(argv[i], "rb", stdin))
        fatal("Cannot open '%s' for reading.\n", argv[i]);
    } else if (! strcmp(argv[i],"-d"))
      dflag = 1;
    else if (argv[i][0] == '-')
      goto usage;
    else if (! path)
      path = fix_path(argv[i]);
    else
      goto usage;
  }
  if (!path) {
  usage:
    doswritehelp();
    exit(EXIT_FAILURE);
  }
#ifdef WIN32
  fflush(stdin);
  _setmode(_fileno(stdin), O_BINARY);
#endif
  if (dflag && (s = strrchr(path, '/'))) {
    *s = 0;
    if ((res = rmkdir(path, 1)) != FR_OK)
      return res;
    *s = '/';
  }
  res = f_open(&fil, path, mode);
  if (res != FR_OK)
    return res;
  for(;;) {
    nread = fread(buffer, 1, sizeof(buffer), stdin);
    if (nread == 0)
      break;
    res = f_write(&fil, buffer, nread, &nwritten);
    if (res != FR_OK)
      break;
    if (nwritten < nread)
      break;
  }
  f_close(&fil);
  if (ferror(stdin))
    fatal("I/O error reading data from stdin\n");
  if (nread && res != FR_OK)
    return res;
  if (nread && nwritten < nread)
    fatal("Filesystem is full\n");
  return FR_OK;
}



/* -------------------------------------------- */
/* DOSMKDIR                                     */
/* -------------------------------------------- */

void dosmkdirhelp(void)
{
  fprintf(stderr,
          "Usage: dosmkdir <options> <path>}}\n"
          "       dosfs --mkdir <options> <path>}}\n"
          "Create a subdirectory named <path>.\n"
          "This command fails if {path} already exists or if its parent\n"
          "directory does not exists. In contrast, with option -q, this\n"
          "command creates all the necessary subdirectories.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-q            :  create all necessary subdirs\n");
}

FRESULT dosmkdir(int argc, const char **argv)
{
  FRESULT res;
  int qflag = 0;
  char *path = 0;
  int i;

  for (i=1; i<argc; i++)
    if (!strcmp(argv[i], "-q"))
      qflag = 1;
    else if (argv[i][0] == '-')
      goto usage;
    else if (!path)
      path = fix_path(argv[i]);
    else
      goto usage;
  if (!path) {
  usage:
    dosmkdirhelp();
    exit(EXIT_FAILURE);
  }
  return rmkdir(path, qflag);
}


/* -------------------------------------------- */
/* DOSDEL                                       */
/* -------------------------------------------- */

void dosdelhelp(void)
{
  fprintf(stderr,
          "Usage: dosdel <options> {<path>[/<pattern>]}\n"
          "       dosfs --del <options> {<path>[/<pattern>]}\n"
          "Delete files or subtrees named <path> or matching <path>/<pattern>.\n"
          "By default this command prompts before deleting subdirectories or files\n"
          "matching a pattern. Use options -i or -q to prompt more or not at all.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-i            :  always prompt before deleting\n"
          "\t-q            :  delete files and trees without prompting\n");
}

FRESULT rdelone(char *path, int verbose);

FRESULT rdelmany(char *path, int verbose)
{
  DIR dir;
  FILINFO info;
  FRESULT res;
  char *pattern;

  if (! pattern_p(path))
    return rdelone(path, verbose);
  else if (! verbose)
    verbose += 1;
  pattern = strrchr(path,'/');    
  if (pattern) {
    *pattern = 0;
    pattern += 1;
  } else {
    pattern = path;
    path = "";
  }
  res = f_findfirst(&dir, &info, path, pattern);
  if (res != FR_OK && res != FR_NO_FILE)
    return res;
  while (res == FR_OK && info.fname[0])
    {
      char *npath = strconcat(path, "/", info.fname, 0);
      res = rdelone(npath, verbose);
      if (res != FR_OK) {
        fprintf(stderr, "dosfs: error while processing %s\n", npath);
        free(npath);
        return res;
      }
      free(npath);
      res = f_findnext(&dir, &info);
    }
  f_closedir(&dir);
  return res;
}

FRESULT rdelone(char *path, int verbose)
{
  if (dir_p(path)) {
    char *npath;
    if (verbose >= 0 && !prompt("[%s]:%s, Delete entire subtree", sfn, path))
      return FR_OK;
    npath = strconcat(path, "/", "*", 0);
    rdelmany(npath, -1);
    free(npath);
  } else if (verbose > 0 && ! prompt("[%s]:%s, Delete", sfn, path))
    return FR_OK;
  return f_unlink(path);
}

FRESULT dosdel(int argc, const char **argv)
{
  int i;
  int verbose = 0;
  FRESULT res = FR_EXIST;
  
  for (i=1; i<argc; i++)
    if (!strcmp(argv[i], "-i"))
      verbose = +1;
    else if (!strcmp(argv[i], "-q"))
      verbose = -1;
    else if (argv[i][0] == '-')
      goto usage;
    else if ((res = rdelmany(fix_path(argv[i]), verbose)) != FR_OK) {
      fprintf(stderr, "dosfs: error while processing '%s'\n", argv[i]);
      return res;
    }
  if (res != FR_OK) {
  usage:
    dosdelhelp();
    exit(EXIT_FAILURE);
  }
  return res;
}


/* -------------------------------------------- */
/* DOSMOVE                                      */
/* -------------------------------------------- */

void dosmovehelp(void)
{
  fprintf(stderr,
          "Usage: dosmove <options> {<src>} <dest>}}\n"
          "       dosfs --move  <options> {<src>} <dest>}}\n"
          "Move or rename files or subtrees.\n"
          "When this command is called with a single source path <src>, the\n"
          "destination <dest> can be an existing directory or give a new filename\n"
          "inside an existing directory. When multiple source paths are given\n"
          "the destination <dest> must be an existing directory.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-q            :  overwrite files without prompting\n");
}

FRESULT dosmove(int argc, const char **argv)
{
  int i;
  int qflag = 0;
  int nargc = 0;
  int dirp = 0;
  char *dest = 0;
  FRESULT res;
  FILINFO info;
  DIR dir;

  for (i = nargc = 1; i < argc; i++)
    if (! strcmp(argv[i], "-q"))
      qflag = 1;
    else if (argv[i][0] == '-')
      goto usage;
    else 
      argv[nargc++] = argv[i];
  if (nargc < 3) {
  usage:
    dosmovehelp();
    exit(EXIT_FAILURE);
  }
  dest = fix_path(argv[argc-1]);
  if (! (dirp = dir_p(dest)))
    if (nargc > 3 || pattern_p(argv[1]))
      fatal("Moving multiple files: Destination must be an existing directory\n");
  for (i = 1; i < nargc - 1; i++) {
    char *src = fix_path(argv[i]);
    char *pattern = strrchr(src, '/');
    if (pattern) {
      *pattern = 0;
      pattern ++;
    } else {
      pattern = src;
      src = "";
    }
    res = f_findfirst(&dir, &info, src, pattern);
    if (res != FR_OK && res != FR_NO_FILE)
      return res;
    while (res == FR_OK && info.fname[0])
      {
        char *from = strconcat(src, "/", info.fname, 0);
        char *to = (dirp) ? strconcat(dest, "/", info.fname, 0) : strdup(dest);
        while (to[0] == '/')
          to += 1;
        if (file_p(to))
          if (qflag || prompt("[%s]:/%s, Replace", sfn, to))
            f_unlink(to);
        res = f_rename(from, to);
        free(from);
        free(to);
        if (res != FR_OK)
          break;
        res = f_findnext(&dir, &info);
      }
    f_closedir(&dir);
    if (res != FR_OK)
      return res;
  }
  return FR_OK;
}

/* -------------------------------------------- */
/* DOSATTRIB                                    */
/* -------------------------------------------- */

void dosattribhelp(void)
{
  fprintf(stderr,
          "Usage: dosattrib <options> [<pattern>]\n"
          "       dosfs --attrib <options> [<pattern>]\n"
          "Display or change file attributes.\n"
          "When used with options [+|-][ARHS], this command changes the\n"
          "attributes of the selected file. When used without options\n"
          "this command prints the attributes.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t+A -A         :  set or remove the archive bit.\n"
          "\t+R -R         :  set or remove the read-only bit.\n"
          "\t+H -H         :  set or remove the hidden bit.\n"
          "\t+S -S         :  set or remove the system bit.\n"
          "\t-s            :  process directories recursively.\n"
          "\t-d            :  change directory attributes.\n");
}

FRESULT rattrib(char *path, char *pattern, BYTE aset, BYTE aclr, int dflag, int sflag, int *pnf)
{
  DIR dir;
  FILINFO info;
  FRESULT res;
  while (path[0] == '/')
    path++;
  res = f_findfirst(&dir, &info, path, pattern);
  if (res != FR_OK && res != FR_NO_FILE)
    return res;
  while (res == FR_OK && info.fname[0])
    {
      if (dflag == 1 || !(info.fattrib & AM_DIR)) {
        *pnf += 1;
        if (aset == 0 && aclr == 0)
          {
            printf("%c%c%c%c %s%s/%s\n",
                   (info.fattrib & AM_ARC) ? 'A' : ' ',
                   (info.fattrib & AM_RDO) ? 'R' : ' ',
                   (info.fattrib & AM_SYS) ? 'S' : ' ',
                   (info.fattrib & AM_HID) ? 'H' : ' ',
                   path[0] ? "/" : "", path, info.fname );
          }
        else
          {
            char *fn = strconcat(path, "/", info.fname, 0);
            res = f_chmod(fn, aset, aset | aclr);
            if (res != FR_OK)
              fprintf(stderr, "dosfs: error while processing %s\n", fn);
            free(fn);
            if (res != FR_OK)
              return res;
          }
      }
      res = f_findnext(&dir, &info);
    }
  if (res != FR_OK)
    return res;
  if (sflag)
    {
      res = f_findfirst(&dir, &info, path, "*");
      if (res != FR_OK && res != FR_NO_FILE)
        return res;
      while (res == FR_OK && info.fname[0])
        {
          if (info.fattrib & AM_DIR) {
            char *fn = strconcat(path, "/", info.fname, 0);
            res = rattrib(fn, pattern, aset, aclr, dflag, sflag, pnf);
            free(fn);
            if (res != FR_OK)
              return res;
          }
          res = f_findnext(&dir, &info);
        }
      if (res != FR_OK)
          return res;
    }
  return FR_OK;
}

FRESULT dosattrib(int argc, const char **argv)
{
  FRESULT res;
  BYTE aset = 0;
  BYTE aclr = 0;
  int dflag = 0;
  int sflag = 0;
  int na = 0;
  int nf = 0;
  int i;

  for (i=1; i < argc; i++)
    {
      BYTE *pflag = 0;
      if (argv[i][0] == '+')
        pflag = &aset;
      else if (argv[i][0] == '-')
        pflag = &aclr;
      if (pflag) {
        if (!strcmp(argv[i],"-d"))
          dflag = 1;
        else if (!strcmp(argv[i],"-s"))
          sflag = 1;
        else if (argv[i][2]) {
        usage:
          dosattribhelp();
          exit(EXIT_FAILURE);
        } else {
          switch(argv[i][1]) {
          case 'A': *pflag |= AM_ARC; break;
          case 'R': *pflag |= AM_RDO; break;
          case 'H': *pflag |= AM_HID; break;
          case 'S': *pflag |= AM_SYS; break;
          default: goto usage;
          }
        }
      } else {
        char *path = fix_path(argv[i]);
        char *pattern = strrchr(path, '/');
        if (dir_p(path) && dflag != 1 && (aset | aclr) == 0) {
          pattern = "*";
        } else if (pattern) {
          *pattern = 0;
          pattern += 1;
        } else {
          pattern = path;
          path = "";
        }
        na += 1;
        res = rattrib(path, pattern, aset, aclr, dflag, sflag, &nf);
        if (res != FR_OK)
          return res;
      }
    }
  if (na == 0 && (aset | aclr) == 0) {
    res = rattrib("", "*", aset, aclr, dflag, sflag, &nf);
    if (res != FR_OK)
      return res;
  }
  if (nf == 0)
    fatal("File not found\n");
  return FR_OK;
}



/* -------------------------------------------- */
/* DOSFORMAT                                    */
/* -------------------------------------------- */

void dosformathelp(void)
{
  fprintf(stderr,
          "Usage: dosformat <options> [<label>]\n"
          "       dosfs --format <options> [<label>]\n"
          "Format a filesystem.\n"
          "With option -p, this command expects a partionned drive and\n"
          "formats the specified partition. Otherwise it formats the entire\n"
          "disk or disk image with a filesystem with or without a partition table.\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-s            :  create a filesystem without a partition table.\n"
          "\t-F <fs>       :  specify a filesystem: FAT, FAT32, or EXFAT.\n");
}

FRESULT dosformat(int argc, const char **argv)
{
  FRESULT res;
  char buffer[64*1024];
  const char *label = 0;
  MKFS_PARM parm;
  int fflag = 0;
  int sflag = 0;
  int i;
  
  memset(&parm, 0, sizeof(parm));
  parm.fmt = FM_ANY;
  parm.n_fat = 2;
  for (i=1; i<argc; i++)
    {
      if (!strcmp(argv[i], "-s")) {
        sflag = 1;
        if (VolToPart[0].pt)
          fatal("Options -s and -p are incompatible.\n");
      } else if (!strcmp(argv[i], "-F")) {
        if (++i >= argc)
          fatal("Option -F requires an argument.\n");
        if (!strcasecmp(argv[i],"fat"))
          fflag |= FM_FAT;
        else if (!strcasecmp(argv[i],"fat32"))
          fflag |= FM_FAT32;
        else if (!strcasecmp(argv[i],"exfat"))
          fflag |= FM_EXFAT;
        else
          fatal("Valid arguments for option -f are: fat fat32 exfat\n");
      } else if (argv[i][0] == '-') {
        goto usage;
      } else if (! label) {
        label = argv[i];
      } else {
      usage:
        dosformathelp();
        exit(EXIT_FAILURE);
      }
    }
  parm.fmt = (fflag) ? fflag : FM_ANY;
  if (sflag)
    parm.fmt |= FM_SFD;

  if (VolToPart[0].pt) {
    if (! prompt("Erase partition %d in [%s]", VolToPart[0].pt, sfn))
      return FR_OK;
  } else {
    if (! prompt("Erase everything in [%s]", sfn))
      return FR_OK;
  }
  res = f_mkfs("", &parm, buffer, sizeof(buffer));
  if (res != FR_OK)
    fatal_code(res);
  if ((res = f_mount(&vol, "", 1)) != FR_OK)
    fatal_code(res);
  if (label)
    res = f_setlabel(label);
  return res;
}


/* -------------------------------------------- */
/* MAIN                                         */
/* -------------------------------------------- */


struct {
  const char *cmd;
  FRESULT (*run)(int, const char **);
  void (*help)(void);
} commands[] = {
                { "dir", dosdir, dosdirhelp },
                { "read", dosread, dosreadhelp },
                { "write", doswrite, doswritehelp },
                { "mkdir", dosmkdir, dosmkdirhelp },
                { "del", dosdel, dosdelhelp },
                { "move", dosmove, dosmovehelp },
                { "attrib", dosattrib, dosattribhelp },
                { "format", dosformat, dosformathelp },
                { 0, 0 } };


int search_cmd(const char *cmd)
{
  int i;

  for (i=0; commands[i].cmd; i++)
    if (! strcmp(cmd, commands[i].cmd))
      return i;
  return -1;
}

void common_usage()
{
  int i;

  fprintf(stderr,
          "Usage: dosfs --<subcmd> <options> <..args..>\n"
          "Usage: dos<subcmd> <options> <..args..>\n"
          "Valid subcommands are:");
  for (i=0; commands[i].cmd; i++)
    fprintf(stderr, " %s", commands[i].cmd);
  fprintf(stderr, "\n");
  fprintf(stderr,
          "Common options:\n");
  common_options();
}

int main(int argc, const char **argv)
{
  int i, j;
  int nargc = 0;
  const char **nargv = argv;
  const char *progname = 0;
  int cmdno = -1;
  int help = 0;
  FRESULT res;
  
  /* try to make utf8 locale */
#ifdef WIN32
  if (setlocale(LC_CTYPE, ".UTF8"))
    recode_arguments_as_utf8(argc, argv, __wargv);
#else
  setlocale(LC_CTYPE, "");
#endif
  if (mblen("\xc2\xa9", 2) != 2 ||
      mblen("\xe2\x89\xa0", 3) != 3)
    warning("The current locale does not seem to be using UTF-8 encoding.\n");
  /* identify progname */
  if ((progname = strrchr(argv[0], '/')))
    progname += 1;
  else
    progname = argv[0];
  /* default command from progname */
  if (! strncmp(progname, "dos", 3))
    cmdno = search_cmd(progname + 3);

  /* parse common options */
  nargv[nargc++] = progname;
  for (int i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1] == '-')
        {
          if (cmdno >= 0)
            warning("Subcommand %s overrides previously selected subcommand --%s\n",
                    argv[i], commands[cmdno].cmd);
          if ((cmdno = search_cmd(argv[i]+2)) < 0)
            fatal("Unrecognized subcommand : %s\n", argv[i]);
          continue;
        }
      if (!strcmp(argv[i], "-f") && i + 1 < argc)
        {
          fn = argv[i + 1];
          sfn = strrchr(fn, '/');
          sfn = (sfn) ? sfn + 1 : fn;
          i += 1;
          continue;
        }
      if (! strcmp(argv[i], "-h"))
        {
          help = 1;
          continue;
        }
#if FF_MULTI_PARTITION
      if (!strcmp(argv[i], "-p") && i + 1 < argc)
        {
          const char *p = argv[i + 1];
          if (p[0] >= '0' && p[0] <= '9' && !p[1])
            VolToPart[0].pt = p[0] - '0';
          else
            fatal("Not a valid partition number: %s\n", p);
          i += 1;
          continue;
        }
#endif
      nargv[nargc++] = argv[i];
    }
  /* Check */
  if (cmdno < 0) {
    common_usage();
    if (help)
      for (i=0; commands[i].cmd; i++) {
        fprintf(stderr,"----------------------------------------\n");
        commands[i].help();
      }
    return EXIT_FAILURE;
  }
  if (help || fn == 0) {
    commands[cmdno].help();
    return EXIT_FAILURE;
  }
  /* Mount, run, unmount */
  if (commands[cmdno].run != dosformat)
    if ((res = f_mount(&vol, "", 1)) != FR_OK)
      fatal_code(res);
  if ((res = commands[cmdno].run(nargc, nargv)) != FR_OK)
    fatal_code(res);
  if ((res = f_mount(NULL, "", 0)) != FR_OK && j >= 0)
    fatal_code(res);
  return EXIT_SUCCESS;
}
