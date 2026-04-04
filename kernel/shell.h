#pragma once
#include "ramdisk.h"

// ---------------------------------------------------------------------------
// NexusOS Shell
// Commands: help, ls, cat, write, mkdir, rm, cd, pwd, clear
// ---------------------------------------------------------------------------

#define SHELL_BUF_SIZE 256

// Shell state
static char shell_buf[SHELL_BUF_SIZE];
static int shell_len = 0;

// ---------------------------------------------------------------------------
// Shell utility functions
// ---------------------------------------------------------------------------

static void shell_int_to_str(uint32_t n, char *buf)
{
    if (n == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[12];
    int i = 0;
    while (n > 0)
    {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static int shell_starts_with(const char *str, const char *prefix)
{
    while (*prefix)
        if (*str++ != *prefix++)
            return 0;
    return 1;
}

// Skip leading spaces, return pointer to first non-space
static const char *shell_skip_spaces(const char *s)
{
    while (*s == ' ')
        s++;
    return s;
}

// Get current working directory path as a string
static void shell_get_cwd(char *buf, int max)
{
    // Walk up to root building path in reverse
    char parts[16][RD_MAX_NAME];
    int depth = 0;
    int node = rd_cwd;

    while (node != 0 && depth < 16)
    {
        rd_strcpy(parts[depth++], rd_nodes[node].name, RD_MAX_NAME);
        node = rd_nodes[node].parent;
    }

    if (depth == 0)
    {
        buf[0] = '/';
        buf[1] = '\0';
        return;
    }

    int pos = 0;
    buf[pos++] = '/';
    for (int i = depth - 1; i >= 0 && pos < max - 2; i--)
    {
        const char *p = parts[i];
        while (*p && pos < max - 2)
            buf[pos++] = *p++;
        if (i > 0)
            buf[pos++] = '/';
    }
    buf[pos] = '\0';
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

static void cmd_help(void)
{
    terminal_write("NexusOS Shell Commands:\n");
    terminal_write("  help               - show this message\n");
    terminal_write("  ls                 - list current directory\n");
    terminal_write("  ls <path>          - list directory at path\n");
    terminal_write("  cat <file>         - print file contents\n");
    terminal_write("  write <file> <txt> - write text to a file\n");
    terminal_write("  mkdir <dir>        - create a directory\n");
    terminal_write("  rmdir <dir>        - delete an empty directory\n");
    terminal_write("  rm <file>          - delete a file or empty dir\n");
    terminal_write("  cd <path>          - change directory\n");
    terminal_write("  pwd                - print working directory\n");
    terminal_write("  clear              - clear the screen\n");
    terminal_write("  sysinfo            - show CPU and RAM info\n");
    terminal_write("  osinfo             - show OS version info\n");
    terminal_write("  gui                - launch graphical desktop\n");
}

static void cmd_ls(const char *path)
{
    int node;
    if (!path || !*path)
        node = rd_cwd;
    else
        node = rd_resolve(path);

    if (node < 0)
    {
        terminal_write("ls: not found\n");
        return;
    }
    if (rd_nodes[node].type != RD_DIR)
    {
        terminal_write("ls: not a directory\n");
        return;
    }

    rd_node_t *dir = &rd_nodes[node];
    if (dir->num_children == 0)
    {
        terminal_write("(empty)\n");
        return;
    }

    for (int i = 0; i < dir->num_children; i++)
    {
        int idx = dir->children[i];
        rd_node_t *child = &rd_nodes[idx];
        if (child->type == RD_DIR)
        {
            terminal_write(child->name);
            terminal_write("/\n");
        }
        else
        {
            terminal_write(child->name);
            terminal_write("  (");
            char size_str[12];
            shell_int_to_str(child->size, size_str);
            terminal_write(size_str);
            terminal_write(" bytes)\n");
        }
    }
}

static void cmd_cat(const char *path)
{
    if (!path || !*path)
    {
        terminal_write("cat: no file specified\n");
        return;
    }

    int idx = rd_resolve(path);
    if (idx < 0)
    {
        terminal_write("cat: file not found\n");
        return;
    }
    if (rd_nodes[idx].type != RD_FILE)
    {
        terminal_write("cat: not a file\n");
        return;
    }

    rd_node_t *f = &rd_nodes[idx];
    if (f->size == 0)
    {
        terminal_write("(empty file)\n");
        return;
    }

    // Print file contents byte by byte
    for (uint32_t i = 0; i < f->size; i++)
        terminal_putchar((char)f->data[i]);
    if (f->data[f->size - 1] != '\n')
        terminal_putchar('\n');
}

static void cmd_write(const char *args)
{
    // args = "<filename> <text...>"
    if (!args || !*args)
    {
        terminal_write("write: usage: write <file> <text>\n");
        return;
    }

    // Extract filename
    char filename[RD_MAX_NAME];
    int fi = 0;
    while (*args && *args != ' ' && fi < RD_MAX_NAME - 1)
        filename[fi++] = *args++;
    filename[fi] = '\0';

    args = shell_skip_spaces(args);
    if (!*args)
    {
        terminal_write("write: no text provided\n");
        return;
    }

    // Write text + newline to file
    int len = rd_strlen(args);
    int idx = rd_resolve(filename);
    if (idx < 0)
        idx = rd_create(filename);
    if (idx < 0)
    {
        terminal_write("write: could not create file\n");
        return;
    }

    rd_nodes[idx].size = 0; // overwrite
    rd_append(filename, args, len);
    rd_append(filename, "\n", 1);
    terminal_write("Written to ");
    terminal_write(filename);
    terminal_write("\n");
}

static void cmd_mkdir(const char *path)
{
    if (!path || !*path)
    {
        terminal_write("mkdir: no name specified\n");
        return;
    }
    if (rd_mkdir(path) < 0)
        terminal_write("mkdir: failed (exists or no space)\n");
    else
    {
        terminal_write("Created ");
        terminal_write(path);
        terminal_write("\n");
    }
}

static void cmd_rm(const char *path)
{
    if (!path || !*path)
    {
        terminal_write("rm: no file specified\n");
        return;
    }
    if (rd_rm(path) < 0)
        terminal_write("rm: failed (not found or dir not empty)\n");
    else
    {
        terminal_write("Deleted ");
        terminal_write(path);
        terminal_write("\n");
    }
}

static void cmd_cd(const char *path)
{
    if (!path || !*path)
    {
        rd_cwd = 0;
        return;
    } // cd with no args = root

    int idx = rd_resolve(path);
    if (idx < 0)
    {
        terminal_write("cd: not found\n");
        return;
    }
    if (rd_nodes[idx].type != RD_DIR)
    {
        terminal_write("cd: not a directory\n");
        return;
    }
    rd_cwd = idx;
}

static void cmd_pwd(void)
{
    char buf[512];
    shell_get_cwd(buf, 512);
    terminal_write(buf);
    terminal_write("\n");
}

static void cmd_rmdir(const char *path)
{
    if (!path || !*path)
    {
        terminal_write("rmdir: no directory specified\n");
        return;
    }

    int idx = rd_resolve(path);
    if (idx < 0)
    {
        terminal_write("rmdir: not found\n");
        return;
    }
    if (rd_nodes[idx].type != RD_DIR)
    {
        terminal_write("rmdir: not a directory (use rm for files)\n");
        return;
    }
    if (rd_nodes[idx].num_children > 0)
    {
        terminal_write("rmdir: directory not empty\n");
        return;
    }
    if (idx == 0)
    {
        terminal_write("rmdir: cannot delete root\n");
        return;
    }
    if (idx == rd_cwd)
    {
        terminal_write("rmdir: cannot delete current directory\n");
        return;
    }

    rd_rm(path);
    terminal_write("Deleted ");
    terminal_write(path);
    terminal_write("\n");
}

static void do_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

static void cmd_sysinfo(void)
{
    terminal_write("== System Information ==\n\n");

    // --- CPU brand string ---
    // CPUID leaves 0x80000002-0x80000004 give the brand string (48 chars)
    uint32_t eax, ebx, ecx, edx;

    // Check if brand string is supported
    do_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004)
    {
        char brand[49];
        uint32_t *b = (uint32_t *)brand;
        do_cpuid(0x80000002, &b[0], &b[1], &b[2], &b[3]);
        do_cpuid(0x80000003, &b[4], &b[5], &b[6], &b[7]);
        do_cpuid(0x80000004, &b[8], &b[9], &b[10], &b[11]);
        brand[48] = '\0';

        // Trim leading spaces
        char *cpu = brand;
        while (*cpu == ' ')
            cpu++;

        terminal_write("CPU:    ");
        terminal_write(cpu);
        terminal_write("\n");
    }
    else
    {
        // Fall back to vendor string (leaf 0)
        do_cpuid(0, &eax, &ebx, &ecx, &edx);
        char vendor[13];
        uint32_t *v = (uint32_t *)vendor;
        v[0] = ebx;
        v[1] = edx;
        v[2] = ecx;
        vendor[12] = '\0';
        terminal_write("CPU vendor: ");
        terminal_write(vendor);
        terminal_write("\n");
    }

    // --- CPU features ---
    do_cpuid(1, &eax, &ebx, &ecx, &edx);

    // Number of logical processors (bits 23:16 of EBX)
    uint32_t logical_cpus = (ebx >> 16) & 0xFF;
    if (logical_cpus == 0)
        logical_cpus = 1;
    char cpu_count[4];
    shell_int_to_str(logical_cpus, cpu_count);
    terminal_write("Cores:  ");
    terminal_write(cpu_count);
    terminal_write(" logical processor(s)\n");

    // Feature flags
    terminal_write("Feats:  ");
    if (edx & (1 << 25))
        terminal_write("SSE ");
    if (edx & (1 << 26))
        terminal_write("SSE2 ");
    if (ecx & (1 << 0))
        terminal_write("SSE3 ");
    if (ecx & (1 << 28))
        terminal_write("AVX ");
    if (edx & (1 << 23))
        terminal_write("MMX ");
    terminal_write("\n");

    // --- Memory: read from Limine memmap ---
    // We expose total_ram from kmain via a global
    extern uint64_t nexus_total_ram;
    uint64_t ram_mb = nexus_total_ram / (1024 * 1024);
    char ram_str[16];
    shell_int_to_str((uint32_t)ram_mb, ram_str);
    terminal_write("RAM:    ");
    terminal_write(ram_str);
    terminal_write(" MB detected\n");

    // --- Architecture ---
    terminal_write("Arch:   x86_64\n");
    terminal_write("Mode:   64-bit protected\n");
}

static void cmd_osinfo(void)
{
    terminal_write("== NexusOS Information ==\n\n");
    terminal_write("OS Name:       NexusOS\n");
    terminal_write("Version:       0.2.0\n");
    terminal_write("Release date:  2026-04-02\n");
    terminal_write("Creator:       Ronnie Harrod & Romeo Walsh\n");
    terminal_write("Architecture:  x86_64\n");
    terminal_write("Bootloader:    Limine v7\n");
    terminal_write("Filesystem:    Ramdisk\n");
    terminal_write("License:       Your own!\n");
    terminal_write("\nBuild info:\n");
    terminal_write("  Compiler:    Clang (freestanding)\n");
    terminal_write("  Linker:      ld.lld\n");
    terminal_write("  Assembler:   NASM\n");
}

// ---------------------------------------------------------------------------
// Shell prompt and input handling
// ---------------------------------------------------------------------------

static void shell_prompt(void)
{
    char cwd[512];
    shell_get_cwd(cwd, 512);
    terminal_write("\nnexus:");
    terminal_write(cwd);
    terminal_write("$ ");
}

static void shell_execute(const char *line)
{
    line = shell_skip_spaces(line);
    if (!*line)
        return;

    if (rd_strcmp(line, "help") == 0)
    {
        cmd_help();
    }
    else if (rd_strcmp(line, "ls") == 0)
    {
        cmd_ls(0);
    }
    else if (shell_starts_with(line, "ls "))
    {
        cmd_ls(shell_skip_spaces(line + 3));
    }
    else if (shell_starts_with(line, "cat "))
    {
        cmd_cat(shell_skip_spaces(line + 4));
    }
    else if (shell_starts_with(line, "write "))
    {
        cmd_write(shell_skip_spaces(line + 6));
    }
    else if (shell_starts_with(line, "mkdir "))
    {
        cmd_mkdir(shell_skip_spaces(line + 6));
    }
    else if (shell_starts_with(line, "rm "))
    {
        cmd_rm(shell_skip_spaces(line + 3));
    }
    else if (shell_starts_with(line, "cd "))
    {
        cmd_cd(shell_skip_spaces(line + 3));
    }
    else if (rd_strcmp(line, "cd") == 0)
    {
        cmd_cd(0);
    }
    else if (rd_strcmp(line, "pwd") == 0)
    {
        cmd_pwd();
    }
    else if (rd_strcmp(line, "clear") == 0)
    {
        // Clear is handled by main.c's terminal — just print enough newlines
        for (int i = 0; i < 50; i++)
            terminal_putchar('\n');
    }
    else if (shell_starts_with(line, "rmdir "))
    {
        cmd_rmdir(shell_skip_spaces(line + 6));
    }
    else if (rd_strcmp(line, "sysinfo") == 0)
    {
        cmd_sysinfo();
    }
    else if (rd_strcmp(line, "osinfo") == 0)
    {
        cmd_osinfo();
    }
    else if (rd_strcmp(line, "gui") == 0)
    {
        gui_run();
        // redraw shell prompt when returning
        terminal_write("NexusOS v0.4.0-alpha\n");
        shell_prompt();
    }
    else
    {
        terminal_write(line);
        terminal_write(": command not found\n");
    }
}

// Call this from your keyboard handler instead of terminal_putchar directly
void shell_handle_key(char c)
{
    if (c == '\n')
    {
        terminal_putchar('\n');
        shell_buf[shell_len] = '\0';
        shell_execute(shell_buf);
        shell_len = 0;
        shell_prompt();
    }
    else if (c == '\b')
    {
        // Backspace
        if (shell_len > 0)
        {
            shell_len--;
            // Erase character on screen (move back, space, move back)
            terminal_putchar('\b');
        }
    }
    else
    {
        if (shell_len < SHELL_BUF_SIZE - 1)
        {
            shell_buf[shell_len++] = c;
            terminal_putchar(c);
        }
    }
}

// Call once from kmain() to start the shell
static void shell_init(void)
{
    rd_init();
    terminal_write("NexusOS Shell - type 'help' for commands");
    shell_prompt();
}