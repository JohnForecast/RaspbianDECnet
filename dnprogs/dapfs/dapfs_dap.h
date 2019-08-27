#ifdef __cplusplus
extern "C" {
#endif
	int dapfs_readdir_dap(const char *path, void *buf, fuse_fill_dir_t filler,
			      off_t offset, struct fuse_file_info *fi);

	int dapfs_getattr_dap(const char *path, struct stat *stbuf);

	int dap_delete_file(const char *path);
	int dap_rename_file(const char *from, const char *to);
	int get_object_info(char *command, char *reply);
	int dap_init(void);

#ifdef __cplusplus
}
#endif


extern char prefix[];
extern int debuglevel;
