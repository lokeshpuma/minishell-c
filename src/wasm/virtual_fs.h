#ifndef MINISHELL_VIRTUAL_FS_H
#define MINISHELL_VIRTUAL_FS_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    VFS_NODE_FILE,
    VFS_NODE_DIR
} vfs_node_type_t;

typedef struct vfs_node vfs_node_t;

struct vfs_node {
    char *name;
    vfs_node_type_t type;
    vfs_node_t *parent;
    /* For directories */
    vfs_node_t **children;
    size_t child_count;
    size_t child_capacity;
    /* For files */
    char *content;
    size_t size;
    size_t capacity;
};

typedef struct {
    vfs_node_t *root;
    char cwd_path[1024];
} virtual_fs_t;

virtual_fs_t *vfs_create(void);
void vfs_destroy(virtual_fs_t *vfs);
void vfs_init_sample_data(virtual_fs_t *vfs);
void vfs_reset(virtual_fs_t *vfs);

int vfs_resolve_path(virtual_fs_t *vfs, const char *path, char *out_canon, size_t out_size);
vfs_node_t *vfs_find_node(virtual_fs_t *vfs, const char *canon_path);

int vfs_read_file(virtual_fs_t *vfs, const char *path, char **out_buf, size_t *out_size);
int vfs_write_file(virtual_fs_t *vfs, const char *path, const char *data, size_t len, bool append);
int vfs_mkdir(virtual_fs_t *vfs, const char *path);
int vfs_touch(virtual_fs_t *vfs, const char *path);
int vfs_rm(virtual_fs_t *vfs, const char *path, bool recursive);
int vfs_cp(virtual_fs_t *vfs, const char *src, const char *dst);
int vfs_mv(virtual_fs_t *vfs, const char *src, const char *dst);
int vfs_chdir(virtual_fs_t *vfs, const char *path);
const char *vfs_getcwd(virtual_fs_t *vfs);

#endif /* MINISHELL_VIRTUAL_FS_H */

