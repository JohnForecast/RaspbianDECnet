struct fd_list
{
    int  in_fd;
    int  out_fd;
    char *remote_name;
};


struct callback_routines
{
    void (*new_caller)(int, int, char *, fd_callback);
    void (*delete_caller)(int);
    void (*show_error)(int, char *);
    void (*write_text)(char *, char*);
    void (*hold)(int, char *);
    void (*answer)(int, int);
    int  (*get_fds)(struct fd_list *);
    void (*lost_server)(void);
    void (*open_display_window)(char *);
    void (*display_line)(char *);
    void (*quit)(void);
};


void  register_callbacks(struct callback_routines *new_cr);
void  do_command(char *cmd);
void  send_data(int fd, char *text, int len);
char *get_local_name(void);
void  close_connection(int);
void  send_hold(int held, int fd);
void  localsock_callback(int fd);
int   init_backend(void);
void  cancel_dial(void);
