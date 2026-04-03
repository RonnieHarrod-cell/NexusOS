#pragma once

// NexusOS Boot Splash - press Enter to continue

void terminal_putchar(char c);
void terminal_write(const char *str);

extern volatile char auth_key;
extern volatile int auth_key_ready;

static void splash_show(void)
{
    for (int i = 0; i < 60; i++) terminal_putchar('\n');

    terminal_write("\n\n");

    terminal_write("  ##   #  #####  #   #  #  #   ###\n");
    terminal_write("  # #  #  #       # #   #  #  #    \n");
    terminal_write("  #  # #  ####     #    #  #   ##  \n");
    terminal_write("  #   ##  #       # #   #  #     #  \n");
    terminal_write("  #    #  #####  #   #   ##   ###\n");
 
    terminal_write("\n");

    terminal_write("            ####     ###\n");
    terminal_write("           #    #   #    \n");
    terminal_write("           #    #    ##  \n");
    terminal_write("           #    #      #    \n");
    terminal_write("            ####    ###\n");
 
    terminal_write("\n\n");
    terminal_write("  +------------------------------------------------+\n");
    terminal_write("  |                                                |\n");
    terminal_write("  |   Version  : 0.2.0                               |\n");
    terminal_write("  |   Created  : Ronnie                            |\n");
    terminal_write("  |   Released : 2026-04-02                        |\n");
    terminal_write("  |   Arch     : x86_64                            |\n");
    terminal_write("  |                                                |\n");
    terminal_write("  +------------------------------------------------+\n");
    terminal_write("\n");
    terminal_write("              Press ENTER to boot...\n");
 
    while (1)
    {
        while (!auth_key_ready);
        char c = auth_key;
        auth_key_ready = 0;
        if (c == '\n') break;
    }

    for (int i = 0; i < 60; i++) terminal_putchar('\n');
}