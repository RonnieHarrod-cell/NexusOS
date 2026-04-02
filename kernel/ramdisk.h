#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Ramdisk Filesystem
// Simple in-memory filesystem — works on any hardware, no drivers needed.
// Files/dirs lost on reboot, but fast and simple.
// ---------------------------------------------------------------------------

#define RD_MAX_NAME 64
#define RD_MAX_NODES 128
#define RD_MAX_FILESIZE 65536 // 64KB max per file
#define RD_MAX_CHILDREN 32    // max files/dirs per directory

typedef enum
{
    RD_FREE = 0,
    RD_FILE,
    RD_DIR,
} rd_node_type_t;

typedef struct rd_node
{
    rd_node_type_t type;
    char name[RD_MAX_NAME];
    uint32_t size;                 // bytes used (files only)
    uint8_t data[RD_MAX_FILESIZE]; // file contents
    int children[RD_MAX_CHILDREN]; // indices into node pool (dirs)
    int num_children;
    int parent; // index of parent node
} rd_node_t;

// Node pool — index 0 is always root "/"
static rd_node_t rd_nodes[RD_MAX_NODES];
static int rd_initialized = 0;

// Current working directory index
static int rd_cwd = 0;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void rd_strcpy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int rd_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int rd_strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}

static void rd_memcpy(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
}

static void rd_memset(void *dst, uint8_t val, uint32_t n)
{
    uint8_t *d = dst;
    while (n--)
        *d++ = val;
}

// Allocate a free node, return its index or -1
static int rd_alloc_node(void)
{
    for (int i = 1; i < RD_MAX_NODES; i++)
        if (rd_nodes[i].type == RD_FREE)
            return i;
    return -1;
}

// Find a child of `parent` with given name, return index or -1
static int rd_find_child(int parent, const char *name)
{
    rd_node_t *p = &rd_nodes[parent];
    for (int i = 0; i < p->num_children; i++)
    {
        int idx = p->children[i];
        if (rd_strcmp(rd_nodes[idx].name, name) == 0)
            return idx;
    }
    return -1;
}

// Resolve an absolute or relative path, return node index or -1
static int rd_resolve(const char *path)
{
    if (!path || !*path)
        return rd_cwd;

    int node = (path[0] == '/') ? 0 : rd_cwd;
    const char *p = (path[0] == '/') ? path + 1 : path;

    while (*p)
    {
        char component[RD_MAX_NAME];
        int ci = 0;
        while (*p && *p != '/' && ci < RD_MAX_NAME - 1)
            component[ci++] = *p++;
        component[ci] = '\0';
        if (*p == '/')
            p++;
        if (!ci || rd_strcmp(component, ".") == 0)
            continue;
        if (rd_strcmp(component, "..") == 0)
        {
            node = rd_nodes[node].parent;
            continue;
        }
        node = rd_find_child(node, component);
        if (node < 0)
            return -1;
    }
    return node;
}

// ---------------------------------------------------------------------------
// Public filesystem API
// ---------------------------------------------------------------------------

static void rd_init(void)
{
    rd_memset(rd_nodes, 0, sizeof(rd_nodes));
    // Set up root directory
    rd_nodes[0].type = RD_DIR;
    rd_nodes[0].name[0] = '/';
    rd_nodes[0].name[1] = '\0';
    rd_nodes[0].parent = 0;
    rd_nodes[0].num_children = 0;
    rd_cwd = 0;
    rd_initialized = 1;
}

// Create a file. Returns node index or -1.
static int rd_create(const char *path)
{
    // Find parent dir and filename
    // Split path into parent + name
    char parent_path[RD_MAX_NAME * 4] = "/";
    char filename[RD_MAX_NAME];
    int len = rd_strlen(path);

    // Find last '/'
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--)
    {
        if (path[i] == '/')
        {
            last_slash = i;
            break;
        }
    }

    if (last_slash <= 0)
    {
        rd_strcpy(filename, path + (last_slash + 1), RD_MAX_NAME);
    }
    else
    {
        rd_strcpy(parent_path, path, last_slash + 1);
        parent_path[last_slash] = '\0';
        rd_strcpy(filename, path + last_slash + 1, RD_MAX_NAME);
    }

    int parent = (path[0] == '/') ? rd_resolve(parent_path) : rd_cwd;
    if (parent < 0 || rd_nodes[parent].type != RD_DIR)
        return -1;
    if (rd_find_child(parent, filename) >= 0)
        return -1; // already exists

    int idx = rd_alloc_node();
    if (idx < 0)
        return -1;

    rd_memset(&rd_nodes[idx], 0, sizeof(rd_node_t));
    rd_nodes[idx].type = RD_FILE;
    rd_nodes[idx].parent = parent;
    rd_strcpy(rd_nodes[idx].name, filename, RD_MAX_NAME);

    rd_node_t *p = &rd_nodes[parent];
    if (p->num_children >= RD_MAX_CHILDREN)
        return -1;
    p->children[p->num_children++] = idx;

    return idx;
}

// Create a directory. Returns node index or -1.
static int rd_mkdir(const char *path)
{
    int idx = rd_create(path); // reuse create logic
    if (idx < 0)
        return -1;
    rd_nodes[idx].type = RD_DIR;
    return idx;
}

// Write data to a file (overwrites). Returns bytes written or -1.
static int rd_write(const char *path, const void *data, uint32_t len)
{
    int idx = rd_resolve(path);
    if (idx < 0)
    {
        // Try creating it
        idx = rd_create(path);
        if (idx < 0)
            return -1;
    }
    if (rd_nodes[idx].type != RD_FILE)
        return -1;
    if (len > RD_MAX_FILESIZE)
        len = RD_MAX_FILESIZE;
    rd_memcpy(rd_nodes[idx].data, data, len);
    rd_nodes[idx].size = len;
    return (int)len;
}

// Append data to a file. Returns bytes written or -1.
static int rd_append(const char *path, const void *data, uint32_t len)
{
    int idx = rd_resolve(path);
    if (idx < 0)
    {
        idx = rd_create(path);
        if (idx < 0)
            return -1;
    }
    if (rd_nodes[idx].type != RD_FILE)
        return -1;
    uint32_t space = RD_MAX_FILESIZE - rd_nodes[idx].size;
    if (len > space)
        len = space;
    rd_memcpy(rd_nodes[idx].data + rd_nodes[idx].size, data, len);
    rd_nodes[idx].size += len;
    return (int)len;
}

// Read file into buf. Returns bytes read or -1.
static int rd_read(const char *path, void *buf, uint32_t len)
{
    int idx = rd_resolve(path);
    if (idx < 0 || rd_nodes[idx].type != RD_FILE)
        return -1;
    uint32_t to_read = rd_nodes[idx].size < len ? rd_nodes[idx].size : len;
    rd_memcpy(buf, rd_nodes[idx].data, to_read);
    return (int)to_read;
}

// Delete a file or empty directory. Returns 0 or -1.
static int rd_rm(const char *path)
{
    int idx = rd_resolve(path);
    if (idx <= 0)
        return -1; // can't delete root
    if (rd_nodes[idx].type == RD_DIR && rd_nodes[idx].num_children > 0)
        return -1; // directory not empty

    // Remove from parent's children list
    int parent = rd_nodes[idx].parent;
    rd_node_t *p = &rd_nodes[parent];
    for (int i = 0; i < p->num_children; i++)
    {
        if (p->children[i] == idx)
        {
            // Shift remaining children left
            for (int j = i; j < p->num_children - 1; j++)
                p->children[j] = p->children[j + 1];
            p->num_children--;
            break;
        }
    }

    rd_memset(&rd_nodes[idx], 0, sizeof(rd_node_t));
    return 0;
}