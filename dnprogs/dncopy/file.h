// Generic class for a file.
#ifndef _CC_FILE_H
#define _CC_FILE_H

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#include <limits.h>

class file
{
 public:
    file();
    virtual ~file() {};

    virtual int   setup_link(unsigned int bufsize, int rfm, int rat, int xfer_mode, int flags, int timeout) = 0;
    virtual int   open(const char *mode) = 0;
    virtual int   open(const char *basename, const char *mode) = 0;
    virtual int   close() = 0;
    virtual int   read(char *buf,  int len) = 0;
    virtual int   write(char *buf, int len) = 0;
    virtual int   next() = 0;
    virtual void  perror(const char *) = 0;
    virtual char *get_basename(int keep_version) = 0;
    virtual char *get_printname() = 0;
    virtual char *get_printname(char *filename) = 0;
    virtual const char *get_format_name() = 0;
    virtual int   get_umask() = 0;
    virtual int   set_umask(int mask) = 0;
    virtual bool  eof() = 0;
    virtual bool  isdirectory() = 0;
    virtual bool  iswildcard() = 0;
    virtual int   max_buffersize(int biggest) = 0;
    virtual void  set_protection(char *vmsprot) {};

// Some constants

    static const int MODE_DEFAULT = -1;
    static const int MODE_RECORD = 1;
    static const int MODE_BLOCK  = 2;

    static const int RAT_DEFAULT = -1; // Use RMS defaults
    static const int RAT_FTN  = 1; // RMS RAT values from fab.h
    static const int RAT_CR   = 2;
    static const int RAT_PRN  = 4;
    static const int RAT_NONE = 0;

    static const int RFM_DEFAULT = -1; // Use RMS defaults
    static const int RFM_UDF = 0; // RMS RFM values from fab.h
    static const int RFM_FIX = 1;
    static const int RFM_VAR = 2;
    static const int RFM_VFC = 3;
    static const int RFM_STM = 4;
    static const int RFM_STMLF = 5;
    static const int RFM_STMCR = 6;

    // user_flags passed to setup_link.
    static const int FILE_FLAGS_RRL = 1;
    static const int FILE_FLAGS_SPOOL = 2;
    static const int FILE_FLAGS_DELETE = 4;

 private:
    // Disable copy constructor
    file(const file &);
};

#endif
