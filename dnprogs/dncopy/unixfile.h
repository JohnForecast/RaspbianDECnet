
class unixfile: public file
{
 public:
    unixfile(const char *name);
    unixfile();
    ~unixfile();

    virtual int setup_link(unsigned int bufsize, int rfm, int rat, int xfer_mode, int flags, int timeout);

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

 protected:
    char   filename[PATH_MAX];
    char   printname[PATH_MAX];
    FILE  *stream;
    int    user_rfm;
    int    user_rat;
    int    transfer_mode;
    char  *record_buffer;
    int    record_ptr;
    int    record_buflen;
    unsigned int block_size;

    static const int RECORD_BUFSIZE = 4096;
};

