
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
/* MESSAGES
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

void fatal_code(DRESULT code)
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
    deault: fatal("Internal error %d\n", (int)code);
    }
  exit(EXIT_FAILURE);
}
  
void common_options(void)
{
  fprintf(stderr,
          "\t-f <filename> :  specifies a device or image file\n"
          "\t-p <partno>   :  specifies a partition number (1..4)\n" );
}



/* -------------------------------------------- */
/* FATFS SUPPORT
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
/* FATFS DISKIO
/* -------------------------------------------- */

const char *fn = "/dev/floppy";
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
  if (wp)
    return RES_WRPRT;
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
        *(LBA_t*)buff = (LBA_t)sz;
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
/* UTILITIES
/* -------------------------------------------- */

char *fix_path(const char *path)
{
  char *s;
  char *np = strdup(path);
  for (s = np; *s; s++)
    if (*s == '\\')
      *s = '/';
  return np;
}

const char *pattern_p(const char *path)
{
  const char *s;
  const char *last = strrchr(path, '/');
  last = (last) ? last+1 : path;
  for(s = last; *s; s++)
    if (*s == '*' || *s == '?')
      return last;
  return 0;
}

const int file_p(const char *path)
{
  FILINFO info;
  if (f_stat(path, &info) == FR_OK)
    if (! (info.fattrib & AM_DIR))
      return 1;
  return 0;
}

const int dir_p(const char *path)
{
  FILINFO info;
  if (path[0] == 0 || path[0] == '/' && path[1] == 0)
    return 1;
  if (f_stat(path, &info) == FR_OK)
    if (info.fattrib & AM_DIR)
      return 1;
  return 0;
}

const char *format_date(WORD date)
{
  static char buffer[12];
  sprintf(buffer, "%02d/%02d/%04d",
          (date >> 5) & 0xf, (date & 0x1f), 1980 + (date >> 9) & 0x3f );
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
  printf("%s %s %-8s %8lld ",
         format_date(inf->fdate), format_time(inf->ftime),
         (inf->fattrib & AM_DIR) ? "<DIR>" : "",
         (long long)(inf->fsize) );
#if FF_USE_LFN
  if (xflag)
    printf("%-12s ", (inf->altname && strcasecmp(inf->fname, inf->altname)) ? inf->altname : "");
  printf("%s\n", inf->fname);
#else
  printf("%s\n", inf->fname);
#endif
}


/* -------------------------------------------- */
/* DOSDIR
/* -------------------------------------------- */

void dosdirhelp(void)
{
  fprintf(stderr,
          "Usage: dosdir <options> <path[/pattern]>\n"
          "       dosfs --dir <options>  <path[/pattern]>\n"
          "List the contents of a directory <path> matching the optional pattern <pattern>\n"
          "Options:\n");
  common_options();
  fprintf(stderr,
          "\t-a            :  display all files including system and hidden ones\n"
          "\t-b            :  only display the full path of each file, one per line\n"
          "\t-x            :  dispaly short file names when they're different\n" );
}

FRESULT dosdir(int argc, const char **argv)
{
  int i;
  int aflag = 0;
  int bflag = 0;
  int xflag = 0;
  char *path  = "";
  char *pattern = 0;
  char label[40];
  DWORD serial;
  DIR dir;
  FILINFO info;
  FRESULT res;
  int ndirs = 0;
  int nfiles = 0;
  FSIZE_t sfiles = 0;
  DWORD ncls;
  FATFS *vol;

  for (i=1; i<argc; i++) {
    if (! strcmp(argv[i], "-a"))
      aflag = 1;
    else if (! strcmp(argv[i], "-b"))
      bflag = 1;
    else if (! strcmp(argv[i], "-x"))
      xflag = 1;
    else if (!path || !path[0])
      path = fix_path(argv[i]);
    else {
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
    printf(" Volume Serial Number is %04X-%04X\n\n", (serial >> 16) & 0xffff, serial & 0xffff);
  }
  if (! bflag) {
    printf(" Directory of %s\n\n", (path && path[0]) ? path : "/");
  }

  res = f_findfirst(&dir, &info, path, pattern);
  if (res != FR_OK && res != FR_NO_FILE)
    return res;
  while (res == FR_OK && info.fname[0])
    {
      if (info.fattrib & AM_DIR) {
        ndirs ++;
      } else {
        nfiles ++;
        sfiles += info.fsize;
      }
      if (aflag || !(info.fattrib & (AM_HID|AM_SYS))) {
        if (bflag)
          printf("%s/%s\n", path, info.fname);
        else
          print_filinfo(&info, xflag);
      }
      res = f_findnext(&dir, &info);
    }
  f_closedir(&dir);
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
/* DOSREAD
/* -------------------------------------------- */

void dosreadhelp(void)
{
  fprintf(stderr,
          "Usage: dosread {<paths>}}\n"
          "       dosfs --read {<paths>}\n"
          "Read the specified {<paths>} and dump them to stdout\n"
          "Options:\n");
  common_options();
}

FRESULT dosread(int argc, const char **argv)
{
  int i;
  FRESULT res;

  if (argc < 2) {
    dosreadhelp();
    exit(EXIT_FAILURE);
  }
#ifdef WIN32
  fflush(stdout);
  _setmode(_fileno(stdout), O_BINARY);
#endif
  for (i=1; i<argc; i++)
    {
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
  return FR_OK;
}


/* -------------------------------------------- */
/* DOSWRITE
/* -------------------------------------------- */





/* -------------------------------------------- */
/* MAIN
/* -------------------------------------------- */


struct {
  const char *cmd;
  FRESULT (*run)(int, const char **);
  void (*help)(void);
} commands[] = {
                { "dir", dosdir, dosdirhelp },
                { "read", dosread, dosreadhelp },
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
          "  Valid subcommands are:");
  for (i=0; commands[i].cmd; i++)
    fprintf(stderr, " %s", commands[i].cmd);
  fprintf(stderr, "\n"
          "  Give a command with option -h see command-specific help\n");
  fprintf(stderr,
          "Common options\n");
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
  if (progname = strrchr(argv[0], '/'))
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
    return EXIT_FAILURE;
  }
  if (help) {
    commands[cmdno].help();
    return EXIT_FAILURE;
  }

  /* Mount, run, unmount */
  if ((res = f_mount(&vol, "", 1)) != FR_OK)
    fatal_code(res);
  if ((res = commands[cmdno].run(nargc, nargv)) != FR_OK)
    fatal_code(res);
  if ((res = f_mount(NULL, "", 0)) != FR_OK && j >= 0)
    fatal_code(res);
  return EXIT_SUCCESS;
}
