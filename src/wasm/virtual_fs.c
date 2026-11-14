#define _POSIX_C_SOURCE 200809L
#include "virtual_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static vfs_node_t *node_create(const char *name, vfs_node_type_t type, vfs_node_t *parent) {
    vfs_node_t *node = (vfs_node_t *)malloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    node->name = strdup(name ? name : "");
    node->type = type;
    node->parent = parent;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->content = NULL;
    node->size = 0;
    node->capacity = 0;
    return node;
}

static void node_destroy(vfs_node_t *node) {
    if (!node) return;
    if (node->name) free(node->name);
    if (node->content) free(node->content);
    if (node->children) {
        for (size_t i = 0; i < node->child_count; i++) {
            node_destroy(node->children[i]);
        }
        free(node->children);
    }
    free(node);
}

static int node_add_child(vfs_node_t *dir, vfs_node_t *child) {
    if (!dir || dir->type != VFS_NODE_DIR || !child) return -1;
    if (dir->child_count >= dir->child_capacity) {
        size_t new_cap = dir->child_capacity == 0 ? 8 : dir->child_capacity * 2;
        vfs_node_t **new_children = (vfs_node_t **)realloc(dir->children, sizeof(vfs_node_t *) * new_cap);
        if (!new_children) return -1;
        dir->children = new_children;
        dir->child_capacity = new_cap;
    }
    dir->children[dir->child_count++] = child;
    child->parent = dir;
    return 0;
}

static int node_remove_child(vfs_node_t *dir, vfs_node_t *child) {
    if (!dir || !child) return -1;
    for (size_t i = 0; i < dir->child_count; i++) {
        if (dir->children[i] == child) {
            for (size_t j = i; j + 1 < dir->child_count; j++) {
                dir->children[j] = dir->children[j + 1];
            }
            dir->child_count--;
            return 0;
        }
    }
    return -1;
}

virtual_fs_t *vfs_create(void) {
    virtual_fs_t *vfs = (virtual_fs_t *)malloc(sizeof(virtual_fs_t));
    if (!vfs) return NULL;
    vfs->root = node_create("", VFS_NODE_DIR, NULL);
    strcpy(vfs->cwd_path, "/");
    return vfs;
}

void vfs_destroy(virtual_fs_t *vfs) {
    if (!vfs) return;
    if (vfs->root) {
        node_destroy(vfs->root);
    }
    free(vfs);
}

int vfs_resolve_path(virtual_fs_t *vfs, const char *path, char *out_canon, size_t out_size) {
    if (!vfs || !path || !out_canon || out_size == 0) return -1;

    char raw[1024];
    if (path[0] == '/') {
        strncpy(raw, path, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';
    } else {
        if (strcmp(vfs->cwd_path, "/") == 0) {
            snprintf(raw, sizeof(raw), "/%s", path);
        } else {
            snprintf(raw, sizeof(raw), "%s/%s", vfs->cwd_path, path);
        }
    }

    /* Tokenize by '/' and process '.' and '..' */
    char *parts[64];
    size_t part_count = 0;

    char *copy = strdup(raw);
    if (!copy) return -1;

    char *token = strtok(copy, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || strlen(token) == 0) {
            /* Ignore */
        } else if (strcmp(token, "..") == 0) {
            if (part_count > 0) {
                part_count--;
            }
        } else {
            if (part_count < 64) {
                parts[part_count++] = token;
            }
        }
        token = strtok(NULL, "/");
    }

    if (part_count == 0) {
        snprintf(out_canon, out_size, "/");
    } else {
        out_canon[0] = '\0';
        for (size_t i = 0; i < part_count; i++) {
            size_t cur_len = strlen(out_canon);
            snprintf(out_canon + cur_len, out_size - cur_len, "/%s", parts[i]);
        }
    }

    free(copy);
    return 0;
}

vfs_node_t *vfs_find_node(virtual_fs_t *vfs, const char *canon_path) {
    if (!vfs || !vfs->root || !canon_path) return NULL;
    if (strcmp(canon_path, "/") == 0) return vfs->root;

    char *copy = strdup(canon_path);
    if (!copy) return NULL;

    vfs_node_t *curr = vfs->root;
    char *token = strtok(copy, "/");
    while (token != NULL && curr != NULL) {
        if (curr->type != VFS_NODE_DIR) {
            free(copy);
            return NULL;
        }
        vfs_node_t *next = NULL;
        for (size_t i = 0; i < curr->child_count; i++) {
            if (strcmp(curr->children[i]->name, token) == 0) {
                next = curr->children[i];
                break;
            }
        }
        curr = next;
        token = strtok(NULL, "/");
    }

    free(copy);
    return curr;
}

int vfs_read_file(virtual_fs_t *vfs, const char *path, char **out_buf, size_t *out_size) {
    if (!vfs || !path || !out_buf || !out_size) return -1;

    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    vfs_node_t *node = vfs_find_node(vfs, canon);
    if (!node || node->type != VFS_NODE_FILE) return -1;

    char *buf = (char *)malloc(node->size + 1);
    if (!buf) return -1;
    if (node->size > 0 && node->content) {
        memcpy(buf, node->content, node->size);
    }
    buf[node->size] = '\0';

    *out_buf = buf;
    *out_size = node->size;
    return 0;
}

int vfs_write_file(virtual_fs_t *vfs, const char *path, const char *data, size_t len, bool append) {
    if (!vfs || !path) return -1;

    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    vfs_node_t *node = vfs_find_node(vfs, canon);
    if (node) {
        if (node->type != VFS_NODE_FILE) return -1;
        if (!append) {
            free(node->content);
            node->content = (char *)malloc(len + 1);
            if (!node->content) return -1;
            if (data && len > 0) memcpy(node->content, data, len);
            node->content[len] = '\0';
            node->size = len;
            node->capacity = len + 1;
        } else {
            size_t new_size = node->size + len;
            char *new_content = (char *)realloc(node->content, new_size + 1);
            if (!new_content) return -1;
            node->content = new_content;
            if (data && len > 0) memcpy(node->content + node->size, data, len);
            node->content[new_size] = '\0';
            node->size = new_size;
            node->capacity = new_size + 1;
        }
        return 0;
    }

    /* File does not exist, find parent directory */
    char dir_path[1024];
    char file_name[256];
    char *last_slash = strrchr(canon, '/');
    if (!last_slash) return -1;

    if (last_slash == canon) {
        strcpy(dir_path, "/");
        strncpy(file_name, last_slash + 1, sizeof(file_name) - 1);
    } else {
        size_t dlen = (size_t)(last_slash - canon);
        memcpy(dir_path, canon, dlen);
        dir_path[dlen] = '\0';
        strncpy(file_name, last_slash + 1, sizeof(file_name) - 1);
    }
    file_name[sizeof(file_name) - 1] = '\0';

    vfs_node_t *parent_dir = vfs_find_node(vfs, dir_path);
    if (!parent_dir || parent_dir->type != VFS_NODE_DIR) return -1;

    vfs_node_t *new_file = node_create(file_name, VFS_NODE_FILE, parent_dir);
    if (!new_file) return -1;

    new_file->content = (char *)malloc(len + 1);
    if (!new_file->content) {
        node_destroy(new_file);
        return -1;
    }
    if (data && len > 0) memcpy(new_file->content, data, len);
    new_file->content[len] = '\0';
    new_file->size = len;
    new_file->capacity = len + 1;

    return node_add_child(parent_dir, new_file);
}

int vfs_mkdir(virtual_fs_t *vfs, const char *path) {
    if (!vfs || !path) return -1;

    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    if (vfs_find_node(vfs, canon) != NULL) {
        return -1; /* Already exists */
    }

    char dir_path[1024];
    char name[256];
    char *last_slash = strrchr(canon, '/');
    if (!last_slash) return -1;

    if (last_slash == canon) {
        strcpy(dir_path, "/");
        strncpy(name, last_slash + 1, sizeof(name) - 1);
    } else {
        size_t dlen = (size_t)(last_slash - canon);
        memcpy(dir_path, canon, dlen);
        dir_path[dlen] = '\0';
        strncpy(name, last_slash + 1, sizeof(name) - 1);
    }
    name[sizeof(name) - 1] = '\0';

    vfs_node_t *parent_dir = vfs_find_node(vfs, dir_path);
    if (!parent_dir || parent_dir->type != VFS_NODE_DIR) return -1;

    vfs_node_t *new_dir = node_create(name, VFS_NODE_DIR, parent_dir);
    if (!new_dir) return -1;

    return node_add_child(parent_dir, new_dir);
}

int vfs_touch(virtual_fs_t *vfs, const char *path) {
    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    vfs_node_t *node = vfs_find_node(vfs, canon);
    if (node) {
        return 0; /* Exists */
    }
    return vfs_write_file(vfs, path, "", 0, false);
}

int vfs_rm(virtual_fs_t *vfs, const char *path, bool recursive) {
    if (!vfs || !path) return -1;

    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    if (strcmp(canon, "/") == 0) return -1; /* Cannot delete root */

    vfs_node_t *node = vfs_find_node(vfs, canon);
    if (!node) return -1;

    if (node->type == VFS_NODE_DIR && node->child_count > 0 && !recursive) {
        return -1; /* Directory not empty */
    }

    vfs_node_t *parent = node->parent;
    if (!parent) return -1;

    node_remove_child(parent, node);
    node_destroy(node);
    return 0;
}

int vfs_cp(virtual_fs_t *vfs, const char *src, const char *dst) {
    char *content = NULL;
    size_t size = 0;
    if (vfs_read_file(vfs, src, &content, &size) != 0) return -1;

    char canon_dst[1024];
    if (vfs_resolve_path(vfs, dst, canon_dst, sizeof(canon_dst)) != 0) {
        free(content);
        return -1;
    }

    vfs_node_t *target_node = vfs_find_node(vfs, canon_dst);
    if (target_node && target_node->type == VFS_NODE_DIR) {
        /* Copy into directory with src basename */
        const char *src_base = strrchr(src, '/');
        src_base = src_base ? src_base + 1 : src;
        char new_dst[1024];
        if (strcmp(canon_dst, "/") == 0) {
            snprintf(new_dst, sizeof(new_dst), "/%s", src_base);
        } else {
            snprintf(new_dst, sizeof(new_dst), "%s/%s", canon_dst, src_base);
        }
        int res = vfs_write_file(vfs, new_dst, content, size, false);
        free(content);
        return res;
    }

    int res = vfs_write_file(vfs, dst, content, size, false);
    free(content);
    return res;
}

int vfs_mv(virtual_fs_t *vfs, const char *src, const char *dst) {
    if (vfs_cp(vfs, src, dst) != 0) return -1;
    return vfs_rm(vfs, src, true);
}

int vfs_chdir(virtual_fs_t *vfs, const char *path) {
    if (!vfs || !path) return -1;

    char canon[1024];
    if (vfs_resolve_path(vfs, path, canon, sizeof(canon)) != 0) return -1;

    vfs_node_t *node = vfs_find_node(vfs, canon);
    if (!node || node->type != VFS_NODE_DIR) return -1;

    strncpy(vfs->cwd_path, canon, sizeof(vfs->cwd_path) - 1);
    vfs->cwd_path[sizeof(vfs->cwd_path) - 1] = '\0';
    return 0;
}

const char *vfs_getcwd(virtual_fs_t *vfs) {
    return vfs ? vfs->cwd_path : "/";
}

void vfs_init_sample_data(virtual_fs_t *vfs) {
    if (!vfs) return;

    vfs_mkdir(vfs, "/home");
    vfs_mkdir(vfs, "/home/user");
    vfs_mkdir(vfs, "/home/user/data");
    vfs_mkdir(vfs, "/etc");
    vfs_mkdir(vfs, "/bin");

    const char *welcome = 
        "Welcome to MiniShell WebAssembly Demo!\n"
        "This interactive demonstration runs C shell logic inside your browser via WebAssembly.\n"
        "Type 'help' to see available built-in commands.\n"
        "Try pipelines and redirection, e.g.:\n"
        "  cat data/fruits.txt | sort | uniq\n"
        "  echo 'hello world' > test.txt\n"
        "  cat < test.txt\n";
    vfs_write_file(vfs, "/home/user/welcome.txt", welcome, strlen(welcome), false);

    const char *notes =
        "Portfolio Project Checklist:\n"
        "- [x] C11 Tokenizer & recursive AST parser\n"
        "- [x] Native POSIX executor (fork, execvp, waitpid, pipe, dup2)\n"
        "- [x] Signal handling (Ctrl+C foreground interrupt)\n"
        "- [x] In-memory Virtual Filesystem (VFS)\n"
        "- [x] WebAssembly compilation via Emscripten\n"
        "- [x] GitHub Pages deployment via GitHub Actions\n";
    vfs_write_file(vfs, "/home/user/notes.txt", notes, strlen(notes), false);

    const char *csv =
        "id,name,role,department\n"
        "101,Alice Johnson,Core Systems Engineer,Kernel\n"
        "102,Bob Smith,UI/UX Designer,Frontend\n"
        "103,Charlie Davis,Compiler Engineer,Runtimes\n"
        "104,Diana Prince,Security Researcher,Infra\n";
    vfs_write_file(vfs, "/home/user/data/sample.csv", csv, strlen(csv), false);

    const char *fruits =
        "apple\n"
        "banana\n"
        "orange\n"
        "apple\n"
        "grape\n"
        "banana\n"
        "pear\n";
    vfs_write_file(vfs, "/home/user/data/fruits.txt", fruits, strlen(fruits), false);

    const char *os_release =
        "NAME=\"MiniShell OS (Virtual)\"\n"
        "VERSION=\"1.0.0\"\n"
        "ID=minishell\n"
        "PRETTY_NAME=\"MiniShell WebAssembly Demo v1.0\"\n";
    vfs_write_file(vfs, "/etc/os-release", os_release, strlen(os_release), false);

    const char *motd =
        "Educational POSIX Shell Demonstration\n"
        "Running safely inside WebAssembly with Virtual In-Memory Filesystem\n";
    vfs_write_file(vfs, "/etc/motd", motd, strlen(motd), false);

    vfs_chdir(vfs, "/home/user");
}

void vfs_reset(virtual_fs_t *vfs) {
    if (!vfs) return;
    if (vfs->root) {
        node_destroy(vfs->root);
    }
    vfs->root = node_create("", VFS_NODE_DIR, NULL);
    strcpy(vfs->cwd_path, "/");
    vfs_init_sample_data(vfs);
}
