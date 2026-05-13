#ifndef _INITFS_H
#define _INITFS_H
#include <sys/defs.h>

#define MAXLEN 100
#define MAXSIZE 100

enum ftype {DIRECTORY, FILE};
enum perm {O_RDONLY, O_WRONLY, O_RDWR, O_APPEND, O_CREAT, O_TRUNC};

typedef struct indexnode {
	int i_perm;
	uint64_t i_start;
	uint64_t i_end;
}inode;

typedef struct dent {
	char d_name[MAXLEN];
	inode *d_ino;
	uint64_t d_begin;
	uint64_t d_end;
	struct dent* d_children[MAXSIZE];
	int d_type;
}dentry;

extern dentry *root_node;

void parse_tarfs();
void initfs();
dentry* dentry_lookup(char *path, uint64_t mode);
char* dentry_lookup_get_path(char* path);
#endif
