#pragma once

// NexusOS Boot Password Protection
// - Shows * for each char typed
// - 3 failed attempts = system halt

#define AUTH_PASSWORD "password"
#define AUTH_MAX_ATTEMPTS 3
#define AUTH_MAX_LEN 64

void terminal_putchar(char c);
void terminal_write(const char *str);

static int auth_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void auth_delay(void)
{
    // burn cycles so halt message is readable
    for (volatile long i = 0; i < 200000000L; i++);
}

static void auth_halt(void)
{
    terminal_write("\n\n");
    terminal_write("  !!! ACCESS DENIED !!!\n");
    terminal_write("  Too many incorrect attempts.\n");
    terminal_write("  System halted.\n");
    auth_delay();
    __asm__ volatile("cli"); // disable interrupts
    while (1) __asm__ volatile("hlt");
}

// banner
static void auth_draw_banner(void)
{
    terminal_write("  +---------------------------------+\n");
    terminal_write("  |          N E X U S O S          |\n");
    terminal_write("  |      Authorised Access Only      |\n");
    terminal_write("  +---------------------------------+\n\n");
}

static void auth_login(void)
{
    int attempts = 0;

    auth_draw_banner();
    
    while (attempts < AUTH_MAX_ATTEMPTS)
    {
        char buf[AUTH_MAX_LEN];
        int len = 0;

        if (attempts > 0)
        {
            terminal_write("  Warning: ");
            char rem[4] = {'0' + (AUTH_MAX_ATTEMPTS - attempts), ' ', 0};
            terminal_write(rem);
            terminal_write("attempt(s) remaining\n\n");
        }

        terminal_write("  Password: ");

        extern volatile char auth_key;
        extern volatile int auth_key_ready;

        while (1)
        {
            while (!auth_key_ready);

            char c = auth_key;
            auth_key_ready = 0;

            if (c == '\n')
            {
                // Enter pressed
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (len > 0) {
                    len--;
                    terminal_putchar('\b');
                }
            } else {
                if (len < AUTH_MAX_LEN - 1) {
                    buf[len++] = c;
                    terminal_putchar('*');
                }
            }
        }

        buf[len] = '\0';

        if (auth_strcmp(buf, AUTH_PASSWORD) == 0)
        {
            terminal_write("\n  Access granted. Welcome, User!\n\n");
            auth_delay();
            // clear screen
            for (int i = 0; i < 60; i++) terminal_putchar('\n');
            return; // success - continue to shell
        }

        attempts++;
        terminal_write("  Incorrect password.\n\n");
    }

    auth_halt();
}