#include <sys/elf64.h>
#include <sys/initfs.h>
#include <sys/tarfs.h>
#include <sys/defs.h>
#include <sys/kprintf.h>
#include <sys/kernel_threads.h>
#include <sys/paging.h>
#include <sys/kmemcpy.h>
#include <sys/kstring.h>
#include <sys/utils.h>

dentry *root_node;
#include <sys/kmalloc.h>

char* dentry_lookup_get_path(char* path) {
	char *ret_path = (char*) kmalloc(sizeof(char) *100);
	memset(ret_path, 0, 100);
	kstrcat(ret_path, "/");
	dentry *iter_node = root_node->d_children[0];
	dentry *temp_node = root_node->d_children[0];
	if(kstrcmp(path, "/") == 0) return ret_path;
	if(path[0] == '/') path = path + 1;
	char *token = kstrtok(path , '/');
	if(token == NULL) return NULL;
	
	while(token != NULL) {
		temp_node = iter_node;
		if (temp_node->d_type == FILE) return NULL;
		if(kstrcmp(token, ".") == 0) {
			iter_node = temp_node->d_children[1];
		}
		else if(kstrcmp(token, "..") == 0) {
			iter_node = temp_node->d_children[0];
			kstrrem(ret_path, '/');	
		}
		else {
			//kstrcat(ret_path, temp_node->d_name);
			int i;
			for (i = 2; i < temp_node->d_end; i++) {
				if(kstrcmp(temp_node->d_children[i]->d_name, token) == 0) {
					iter_node = temp_node->d_children[i];
					break;
				}
			}
			if(i == temp_node->d_end) {
				return NULL;
			}
			kstrcat(ret_path, iter_node->d_name);
			kstrcat(ret_path, "/");
		}
		token = kstrtok(NULL, '/');
	}
	if(iter_node->d_type == DIRECTORY)
		return ret_path;
	return NULL;
}

dentry* dentry_lookup(char* path, uint64_t mode) {
	dentry *iter_node = root_node->d_children[0];
	dentry *temp_node = root_node->d_children[0];
	if(kstrcmp(path, "/") == 0) return root_node;
	if(path[0] == '/') path = path + 1;
	char *token = kstrtok(path , '/');
	if(token == NULL) return NULL;
	
	while(token != NULL) {
		temp_node = iter_node;
		if (temp_node->d_type == FILE) return NULL;
		if (temp_node->d_ino->i_perm != mode) return NULL;
		if (kstrcmp(token, ".") == 0) {
			iter_node = temp_node->d_children[1];
		}
		else if(kstrcmp(token, "..") == 0) {
			iter_node = temp_node->d_children[0];
		}
		else {
			int i;
			for (i = 2; i < temp_node->d_end; i++) {
				if(kstrcmp(temp_node->d_children[i]->d_name, token) == 0) {
					iter_node = temp_node->d_children[i];
					break;
				}
			}
			if(i == temp_node->d_end) {
				return NULL;
			}
		}
		token = kstrtok(NULL, '/');
	}
	return iter_node; 
}

inode *make_inode(uint64_t start, uint64_t end, int perm) {
	inode *ino = (inode*) kmalloc(sizeof(inode));
	ino->i_start = start;
	ino->i_end = end;
	ino->i_perm = perm;
	return ino;
}

void make_dentry(dentry *parent, dentry *child, char* name, uint64_t begin, uint64_t end, int type, inode *in) {
	
	memset(child->d_name, 0, MAXLEN);
	kstrcpy(child->d_name, name);
	//kprintf("%s", child->d_name);
        child->d_ino = in;
        child->d_begin = begin;
        child->d_end = end;
        child->d_children[0] = parent;
        child->d_children[1] = child;
        child->d_type = type;

}

void print_dentries(dentry * temp) {
	kprintf("\t%s", temp->d_name);
	if(temp->d_type == DIRECTORY) {
		for(int i =2; i < temp->d_end; i++) {
			print_dentries(temp->d_children[i]);
		}
	}
}

void populate_dentry(char *name, int type, uint64_t start, uint64_t end) {
	dentry *temp_node, *iter_node;
	temp_node = root_node->d_children[2];
	iter_node = root_node->d_children[2];
	char *token = kstrtok(name, '/');
	int i;
	while(token != NULL) {
		//kprintf("\tprint %s", token);
		//if(token[0] == '\0') continue;
		temp_node = iter_node;
		for (i = 2; i < temp_node->d_end; i++) {
			if(kstrcmp(temp_node->d_children[i]->d_name, token) == 0) {
				iter_node = temp_node->d_children[i];
				break;
			}
		}
		if(i == temp_node->d_end) {
			iter_node = (dentry *)kmalloc(sizeof(dentry));
			make_dentry(temp_node, iter_node, token, start, end, type, make_inode(start, end, O_RDONLY));
			temp_node->d_children[i] = iter_node;
			temp_node->d_end++;
		}
		kfree((uint64_t*)token);
		token = kstrtok(NULL, '/');
	}
}
void parse_tarfs() {
        struct posix_header_ustar * header = (struct posix_header_ustar *)&_binary_tarfs_start;
        while(header<(struct posix_header_ustar *)&_binary_tarfs_end) {
                uint64_t size = octalToDecimal(stoi(header->size));
		if(header->name[0] == 0) break;
		if (kstrcmp(header->typeflag, "5") == 0) {
			populate_dentry(header->name, DIRECTORY, 0, 2); 
		} else {
			populate_dentry(header->name, FILE, (uint64_t)(header + 1), (uint64_t)((void *)header + 512 + size));
		}
                if (size == 0) {
                        //kprintf("\nFileName: %s\n", header->name);
			header++;
                }
                else {
                        //kprintf("\nFileName: %s\n", header->name);
			size = (size%512==0) ? size +512: size + 512 + (512-size%512);
			header = (struct posix_header_ustar *) (((uint64_t)(header)) + size);
		}
	}
}

void initfs() {
	root_node = (dentry *) kmalloc (sizeof(dentry));
	make_dentry(root_node, root_node, (char*)"/", 0, 2, DIRECTORY, make_inode(0, 0, O_RDONLY));
	dentry *temp_node = (dentry*) kmalloc(sizeof(dentry));
	make_dentry(root_node, temp_node, (char *)"rootfs", 0, 2, DIRECTORY, make_inode(0, 0, O_RDONLY));
	root_node->d_children[2] = temp_node;
	root_node->d_end = 3;
	parse_tarfs();
	//print_dentries(temp_node);
} 
