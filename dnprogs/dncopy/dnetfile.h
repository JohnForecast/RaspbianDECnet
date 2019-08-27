#include "file.h"

class dnetfile: public file
{
public:

// Is this one of my filenames?
    static bool   isMine(const char *name);

// Constructor and destructor
    dnetfile(const char *name, int verbosity);
    ~dnetfile();

// Stuff overriden from file.
    virtual int   setup_link(unsigned int bufsize, int rfm, int rat, int xfer_mode, int flags, int timeout);
    virtual int   open(const char *mode);
    virtual int   open(const char *basename, const char *mode);
    virtual int   close();
    virtual int   read(char *buf,  int len);
    virtual int   write(char *buf, int len);
    virtual int   next();
    virtual void  perror(const char *);
    virtual char *get_basename(int keep_version);
    virtual char *get_printname();
    virtual char *get_printname(char *filename);
    virtual const char *get_format_name();
    virtual int   get_umask();
    virtual int   set_umask(int mask);
    virtual bool  eof();
    virtual bool  isdirectory();
    virtual bool  iswildcard();
    virtual int   max_buffersize(int biggest);
    virtual void  set_protection(char *prot);

 private:
/* Parameters */
    static const int MAX_NODE      = 6;
    static const int MAX_USER      = 12;
    static const int MAX_PASSWORD  = 12;
    static const int MAX_ACCOUNT   = 12;
    static const int MAX_NAME      = PATH_MAX;
    static const int MAX_BASENAME  = 76;

/* Misc class-globals */
    bool  wildcard; // Is a wildcard file name
    bool  isOpen;   // Set when we have an open connection
    bool  writing;  // if FALSE then we are reading.
    const char *lasterror;
    char  errstring[80];
    int   verbose;

/* File attributes, requested and actual */
    int          file_rat, file_rfm;
    int          user_rat, user_rfm;
    int          user_flags;
    int          transfer_mode;
    int          file_fsz; // Size of VFC fixed part.
    unsigned int user_bufsize;
    dap_connection conn;

/* Connection attribute strings */
    char  fname[MAX_NAME+1]; // Full name as supplied by the user
    char  node[MAX_NODE+1];
    char  user[MAX_USER+1];
    char  password[MAX_PASSWORD+1];
    char  name[MAX_NAME+1];
    char  basename[MAX_BASENAME+1];

    bool  ateof;
    unsigned int prot;
    char *protection; /* VMS style protection string from cmdline */

    char  filname[80];
    char  volname[80];
    char  dirname[80];

/* Filename handling */
    void make_basename(int keep_version);

/* DAP protocol functions */
    void  dap_close_link();
    int   dap_send_access();
    int   dap_get_reply();
    int   dap_get_file_entry(int *rfm, int *rat);
    int   dap_send_connect();
    int   dap_send_get_or_put();
    int   dap_send_accomp();
    int   dap_get_record(char *rec,int reclen);
    int   dap_put_record(char *rec,int reclen);
    int   dap_send_attributes();
    int   dap_send_name();
    int   dap_get_status();
    int   dap_check_status(dap_message *m, int status);
    int   dap_send_skip();
    char *dap_error(int code);

};
