/* configfile.h */
#ifdef __cplusplus
extern "C"
{
#endif
void read_configfile(void);

extern char config_hostname[1024];
extern char config_vmsmailuser[1024];
extern char config_smtphost[1024];

#ifdef __cplusplus
}
#endif
