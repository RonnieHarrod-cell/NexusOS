#pragma once
#include <stdint.h>
#include "mouse.h"
#include "io.h"

// ---------------------------------------------------------------------------
// NexusOS GUI — dark desktop with taskbar and mouse cursor
// Type 'gui' in shell to enter, ESC to return to shell
// ---------------------------------------------------------------------------

// Provided by main.c
extern uint32_t *fb;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint64_t nexus_total_ram;

void terminal_putchar(char c);
void terminal_write(const char *str);

extern volatile char auth_key;
extern volatile int auth_key_ready;

// ---------------------------------------------------------------------------
// Colours (0xRRGGBB packed as 0x00RRGGBB in 32-bit)
// ---------------------------------------------------------------------------
#define COL_DESKTOP 0x1a1a1a
#define COL_TASKBAR 0x222222
#define COL_TASKBAR_BD 0x333333
#define COL_BTN 0x333333
#define COL_BTN_HOV 0x444444
#define COL_BTN_BD 0x444444
#define COL_TEXT 0xcccccc
#define COL_TEXT_DIM 0x888888
#define COL_TEXT_HINT 0x555555
#define COL_CURSOR 0xffffff
#define COL_CURSOR_OUT 0x000000
#define COL_GREEN 0x00ff41
#define COL_BLUE 0x4a9eff
#define COL_WIN_BG 0x2a2a2a
#define COL_WIN_TIT 0x333333
#define COL_WIN_BD 0x3a3a3a

#define TASKBAR_H 32
#define CURSOR_W 12
#define CURSOR_H 20

// ---------------------------------------------------------------------------
// Drawing primitives (operate on global fb)
// ---------------------------------------------------------------------------
static void gui_pixel(int x, int y, uint32_t col)
{
    if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height)
        return;
    fb[y * (fb_pitch / 4) + x] = col;
}

static void gui_rect(int x, int y, int w, int h, uint32_t col)
{
    for (int row = 0; row < h; row++)
        for (int col2 = 0; col2 < w; col2++)
            gui_pixel(x + col2, y + row, col);
}

static void gui_rect_outline(int x, int y, int w, int h, uint32_t col)
{
    for (int i = 0; i < w; i++)
    {
        gui_pixel(x + i, y, col);
        gui_pixel(x + i, y + h - 1, col);
    }
    for (int i = 0; i < h; i++)
    {
        gui_pixel(x, y + i, col);
        gui_pixel(x + w - 1, y + i, col);
    }
}

// Draw a single character using the kernel font (8x8)
extern const uint8_t font[95][8];

static void gui_char(int x, int y, char c, uint32_t col)
{
    if (c < 32 || c > 126)
        return;
    for (int row = 0; row < 8; row++)
    {
        uint8_t line = font[c - 32][row];
        for (int bit = 0; bit < 8; bit++)
            if (line & (1 << (7 - bit)))
                gui_pixel(x + bit, y + row, col);
    }
}

static void gui_text(int x, int y, const char *str, uint32_t col)
{
    while (*str)
    {
        gui_char(x, y, *str++, col);
        x += 8;
    }
}

static int gui_strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}

static void gui_text_centered(int x, int y, int w, const char *str, uint32_t col)
{
    int len = gui_strlen(str) * 8;
    gui_text(x + (w - len) / 2, y, str, col);
}

static void gui_int(int x, int y, uint32_t n, uint32_t col)
{
    char buf[12];
    int i = 0;
    if (n == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (n > 0)
        {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    // reverse
    for (int a = 0, b = i - 1; a < b; a++, b--)
    {
        char t = buf[a];
        buf[a] = buf[b];
        buf[b] = t;
    }
    buf[i] = '\0';
    gui_text(x, y, buf, col);
}

// ---------------------------------------------------------------------------
// Mouse cursor (arrow shape)
// ---------------------------------------------------------------------------
static int gui_prev_cx = -1, gui_prev_cy = -1;
static uint32_t gui_cursor_save[CURSOR_W * CURSOR_H];

static void gui_cursor_save_bg(int cx, int cy)
{
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            int sx = cx + col, sy = cy + row;
            if (sx >= 0 && sy >= 0 && (uint32_t)sx < fb_width && (uint32_t)sy < fb_height)
                gui_cursor_save[row * CURSOR_W + col] = fb[sy * (fb_pitch / 4) + sx];
            else
                gui_cursor_save[row * CURSOR_W + col] = 0;
        }
}

static void gui_cursor_restore_bg(int cx, int cy)
{
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            int sx = cx + col, sy = cy + row;
            if (sx >= 0 && sy >= 0 && (uint32_t)sx < fb_width && (uint32_t)sy < fb_height)
                fb[sy * (fb_pitch / 4) + sx] = gui_cursor_save[row * CURSOR_W + col];
        }
}

// Simple arrow cursor shape
static const uint8_t cursor_shape[CURSOR_H][CURSOR_W] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0},
    {1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static void gui_draw_cursor(int cx, int cy)
{
    gui_cursor_save_bg(cx, cy);
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            uint8_t v = cursor_shape[row][col];
            if (v == 1)
                gui_pixel(cx + col, cy + row, COL_CURSOR_OUT);
            else if (v == 2)
                gui_pixel(cx + col, cy + row, COL_CURSOR);
        }
    gui_prev_cx = cx;
    gui_prev_cy = cy;
}

static void gui_erase_cursor(void)
{
    if (gui_prev_cx >= 0)
        gui_cursor_restore_bg(gui_prev_cx, gui_prev_cy);
}

// ---------------------------------------------------------------------------
// Taskbar
// ---------------------------------------------------------------------------
#define TASKBAR_Y ((int)(fb_height - TASKBAR_H))

typedef struct
{
    int x, w;
    const char *label;
} gui_btn_t;

#define MAX_BTNS 8
static gui_btn_t gui_btns[MAX_BTNS];
static int gui_num_btns = 0;

static void gui_btn_add(int x, int w, const char *label)
{
    if (gui_num_btns >= MAX_BTNS)
        return;
    gui_btns[gui_num_btns].x = x;
    gui_btns[gui_num_btns].w = w;
    gui_btns[gui_num_btns].label = label;
    gui_num_btns++;
}

static void gui_draw_btn(int idx, int hovered)
{
    gui_btn_t *b = &gui_btns[idx];
    uint32_t bg = hovered ? COL_BTN_HOV : COL_BTN;
    gui_rect(b->x, TASKBAR_Y + 6, b->w, TASKBAR_H - 12, bg);
    gui_rect_outline(b->x, TASKBAR_Y + 6, b->w, TASKBAR_H - 12, COL_BTN_BD);
    gui_text_centered(b->x, TASKBAR_Y + 12, b->w, b->label, COL_TEXT);
}

static int gui_btn_hit(int idx, int mx, int my)
{
    gui_btn_t *b = &gui_btns[idx];
    return mx >= b->x && mx < b->x + b->w &&
           my >= TASKBAR_Y + 6 && my < TASKBAR_Y + TASKBAR_H - 6;
}

// ---------------------------------------------------------------------------
// App launcher overlay
// ---------------------------------------------------------------------------
static int gui_launcher_open = 0;
#define LAUNCHER_W 200
#define LAUNCHER_H 160
#define LAUNCHER_X 8
#define LAUNCHER_ITEMS 4

static const char *launcher_items[LAUNCHER_ITEMS] = {
    "TERMINAL",
    "SYSINFO",
    "OSINFO",
    "CLOSE GUI",
};

static int launcher_sel = 0;

static void gui_draw_launcher(void)
{
    int lx = LAUNCHER_X;
    int ly = TASKBAR_Y - LAUNCHER_H - 4;
    gui_rect(lx, ly, LAUNCHER_W, LAUNCHER_H, COL_WIN_BG);
    gui_rect_outline(lx, ly, LAUNCHER_W, LAUNCHER_H, COL_WIN_BD);
    gui_text(lx + 8, ly + 8, "APPLICATIONS", COL_TEXT_DIM);

    for (int i = 0; i < LAUNCHER_ITEMS; i++)
    {
        int iy = ly + 24 + i * 28;
        uint32_t bg = (i == launcher_sel) ? COL_BTN_HOV : COL_WIN_BG;
        gui_rect(lx + 4, iy, LAUNCHER_W - 8, 24, bg);
        gui_text(lx + 12, iy + 8, launcher_items[i], COL_TEXT);
    }
}

// ---------------------------------------------------------------------------
// Terminal window
// ---------------------------------------------------------------------------
static int gui_term_open = 0;
#define TERM_X 60
#define TERM_Y 60
#define TERM_W 400
#define TERM_H 220
#define TERM_ROWS 20
#define TERM_COLS 48

static char term_lines[TERM_ROWS][TERM_COLS + 1];
static int term_row = 0;
static int term_col = 0;
static char term_input[TERM_COLS];
static int term_input_len = 0;

static void gui_term_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
    {
        for (int c = 0; c <= TERM_COLS; c++)
            term_lines[r][c] = 0;
    }
    term_row = 0;
    term_col = 0;
}

static void gui_term_putchar(char c)
{
    if (c == '\n' || term_col >= TERM_COLS)
    {
        term_col = 0;
        term_row++;
        if (term_row >= TERM_ROWS)
        {
            // scroll
            for (int r = 0; r < TERM_ROWS - 1; r++)
                for (int c = 0; c <= TERM_COLS; c++)
                    term_lines[r][c] = term_lines[r + 1][c];
            for (int c = 0; c <= TERM_COLS; c++)
                term_lines[TERM_ROWS - 1][c] = 0;
            term_row = TERM_ROWS - 1;
        }
        if (c == '\n')
            return;
    }
    term_lines[term_row][term_col++] = c;
    term_lines[term_row][term_col] = '\0';
}

static void gui_term_write(const char *s)
{
    while (*s)
        gui_term_putchar(*s++);
}

static void gui_draw_term(void)
{
    // Window chrome
    gui_rect(TERM_X, TERM_Y, TERM_W, TERM_H, COL_WIN_BG);
    gui_rect(TERM_X, TERM_Y, TERM_W, 18, COL_WIN_TIT);
    gui_rect_outline(TERM_X, TERM_Y, TERM_W, TERM_H, COL_WIN_BD);
    gui_text(TERM_X + 8, TERM_Y + 5, "TERMINAL", COL_TEXT_DIM);

    // Close button
    gui_rect(TERM_X + TERM_W - 20, TERM_Y + 3, 16, 12, 0x992222);
    gui_text(TERM_X + TERM_W - 16, TERM_Y + 5, "X", COL_TEXT);

    // Content area
    gui_rect(TERM_X + 2, TERM_Y + 20, TERM_W - 4, TERM_H - 22, 0x0d1117);

    // Draw text lines
    for (int r = 0; r < TERM_ROWS; r++)
        if (term_lines[r][0])
            gui_text(TERM_X + 6, TERM_Y + 22 + r * 9, term_lines[r], COL_GREEN);

    // Input line
    int input_y = TERM_Y + 22 + term_row * 9;
    gui_text(TERM_X + 6, input_y, "$ ", COL_GREEN);
    gui_text(TERM_X + 22, input_y, term_input, COL_GREEN);
    // Cursor blink (always on for simplicity)
    gui_rect(TERM_X + 22 + term_input_len * 8, input_y, 6, 8, COL_GREEN);
}

// ---------------------------------------------------------------------------
// Sysinfo window
// ---------------------------------------------------------------------------
static int gui_sysinfo_open = 0;
#define SYS_X 100
#define SYS_Y 80
#define SYS_W 320
#define SYS_H 180

static void gui_draw_sysinfo(void)
{
    gui_rect(SYS_X, SYS_Y, SYS_W, SYS_H, COL_WIN_BG);
    gui_rect(SYS_X, SYS_Y, SYS_W, 18, COL_WIN_TIT);
    gui_rect_outline(SYS_X, SYS_Y, SYS_W, SYS_H, COL_WIN_BD);
    gui_text(SYS_X + 8, SYS_Y + 5, "SYSTEM INFO", COL_TEXT_DIM);
    gui_rect(SYS_X + SYS_W - 20, SYS_Y + 3, 16, 12, 0x992222);
    gui_text(SYS_X + SYS_W - 16, SYS_Y + 5, "x", COL_TEXT);

    int tx = SYS_X + 16, ty = SYS_Y + 28;
    gui_text(tx, ty, "OS:    NEXUS V0.3.0-alpha", COL_TEXT);
    gui_text(tx, ty + 18, "ARCH:  X86_64", COL_BLUE);
    gui_text(tx, ty + 36, "BOOT:  LIMINE V7", COL_TEXT);

    uint32_t ram_mb = (uint32_t)(nexus_total_ram / (1024 * 1024));
    gui_text(tx, ty + 54, "RAM:   ", COL_TEXT_DIM);
    gui_int(tx + 56, ty + 54, ram_mb, COL_BLUE);
    gui_text(tx + 56 + 48, ty + 54, " MB", COL_BLUE);

    gui_text(tx, ty + 72, "FS:    RAMDISK", COL_TEXT);
    gui_text(tx, ty + 90, "By:    RONNIE & ROMEO", COL_TEXT_DIM);
}

// ---------------------------------------------------------------------------
// Desktop
// ---------------------------------------------------------------------------
static void gui_draw_desktop(void)
{
    // Background
    gui_rect(0, 0, fb_width, fb_height - TASKBAR_H, COL_DESKTOP);

    // Subtle grid pattern
    for (uint32_t x = 0; x < fb_width; x += 40)
        for (uint32_t y = 0; y < fb_height - TASKBAR_H; y += 40)
            gui_pixel(x, y, 0x222222);

    // Watermark
    gui_text_centered(0, (fb_height - TASKBAR_H) / 2 - 4,
                      fb_width, "NEXUSOS DESKTOP", COL_TEXT_HINT);

    // ESC hint
    gui_text_centered(0, (fb_height - TASKBAR_H) / 2 + 12,
                      fb_width, "ESC = RETURN TO SHELL", 0x333333);
}

static void gui_draw_taskbar(int mx, int my)
{
    gui_rect(0, TASKBAR_Y, fb_width, TASKBAR_H, COL_TASKBAR);
    // Top border
    for (uint32_t x = 0; x < fb_width; x++)
        gui_pixel(x, TASKBAR_Y, COL_TASKBAR_BD);

    // Draw buttons
    for (int i = 0; i < gui_num_btns; i++)
        gui_draw_btn(i, gui_btn_hit(i, mx, my));

    // CPU/RAM readout (center)
    gui_text_centered(200, TASKBAR_Y + 12, (int)fb_width - 400,
                      "CPU --  RAM --", COL_TEXT_HINT);

    // Clock (right side) — static for now, RTC would need extra driver
    gui_text((int)fb_width - 72, TASKBAR_Y + 12, "NEXUSOS", COL_TEXT_DIM);
}

// ---------------------------------------------------------------------------
// Main GUI loop — call from shell with 'gui' command
// Returns when user presses ESC
// ---------------------------------------------------------------------------
static void gui_run(void)
{
    // Init mouse
    mouse_init((int)fb_width, (int)fb_height);

    // Enable IRQ12 (mouse) — update PIC mask
    // Current mask: 0xFD (only IRQ1 unmasked). Add IRQ12 = unmask bit 4 on slave.
    outb(0x21, 0xF9); // master: only IRQ1
    outb(0xA1, 0xEF); // slave: unmask IRQ12 (bit 4 = 0), rest masked

    // Build taskbar buttons
    gui_num_btns = 0;
    gui_btn_add(8, 80, "  APPS");
    gui_btn_add(96, 80, "TERMINAL");
    gui_btn_add(184, 60, " FILES");

    // Initial draw
    gui_term_clear();
    gui_term_write("NEXUSOS TERMINAL\n");
    gui_term_write("$ ");

    gui_draw_desktop();
    gui_draw_taskbar(mouse_x, mouse_y);
    if (gui_term_open)
        gui_draw_term();
    if (gui_sysinfo_open)
        gui_draw_sysinfo();
    if (gui_launcher_open)
        gui_draw_launcher();
    gui_draw_cursor(mouse_x, mouse_y);

    int prev_mx = mouse_x, prev_my = mouse_y;
    int prev_left = 0;
    int needs_redraw = 0;

    while (1)
    {
        needs_redraw = 0;

        // Check keyboard (ESC = exit GUI)
        if (auth_key_ready)
        {
            char c = auth_key;
            auth_key_ready = 0;

            if (c == 27)
            { // ESC
                // Restore terminal screen
                for (int i = 0; i < 60; i++)
                    terminal_putchar('\n');
                // Re-mask mouse IRQ
                outb(0xA1, 0xFF);
                return;
            }

            // Handle launcher keyboard nav
            if (gui_launcher_open)
            {
                if (c == '\n')
                {
                    gui_launcher_open = 0;
                    if (launcher_sel == 0)
                    {
                        gui_term_open = 1;
                    }
                    else if (launcher_sel == 1)
                    {
                        gui_sysinfo_open = 1;
                    }
                    else if (launcher_sel == 2)
                    {
                        gui_sysinfo_open = 1;
                    }
                    else if (launcher_sel == 3)
                    {
                        for (int i = 0; i < 60; i++)
                            terminal_putchar('\n');
                        outb(0xA1, 0xFF);
                        return;
                    }
                    needs_redraw = 1;
                }
            }
            else if (gui_term_open && c != 27)
            {
                // Type into terminal window
                if (c == '\n')
                {
                    gui_term_putchar('\n');
                    term_input[term_input_len] = '\0';
                    // Simple built-in commands
                    if (term_input[0] == 'c' && term_input[1] == 'l' && term_input[2] == 's')
                    {
                        gui_term_clear();
                    }
                    else
                    {
                        gui_term_write(term_input);
                        gui_term_write(": NOT FOUND\n");
                    }
                    term_input_len = 0;
                    term_input[0] = '\0';
                    gui_term_write("$ ");
                }
                else if (c == '\b' && term_input_len > 0)
                {
                    term_input[--term_input_len] = '\0';
                }
                else if (term_input_len < TERM_COLS - 4)
                {
                    term_input[term_input_len++] = c;
                    term_input[term_input_len] = '\0';
                }
                needs_redraw = 1;
            }
        }

        // Check mouse movement
        int cx = mouse_x, cy = mouse_y;
        int left = mouse_left;

        // Click handling (rising edge of left button)
        if (left && !prev_left)
        {
            // Taskbar buttons
            for (int i = 0; i < gui_num_btns; i++)
            {
                if (gui_btn_hit(i, cx, cy))
                {
                    if (i == 0)
                    { // Apps
                        gui_launcher_open = !gui_launcher_open;
                    }
                    else if (i == 1)
                    { // Terminal
                        gui_term_open = !gui_term_open;
                        gui_launcher_open = 0;
                    }
                    else if (i == 2)
                    { // Files — open sysinfo for now
                        gui_sysinfo_open = !gui_sysinfo_open;
                        gui_launcher_open = 0;
                    }
                    needs_redraw = 1;
                }
            }

            // Close buttons on windows
            if (gui_term_open &&
                cx >= TERM_X + TERM_W - 20 && cx < TERM_X + TERM_W - 4 &&
                cy >= TERM_Y + 3 && cy < TERM_Y + 15)
            {
                gui_term_open = 0;
                needs_redraw = 1;
            }
            if (gui_sysinfo_open &&
                cx >= SYS_X + SYS_W - 20 && cx < SYS_X + SYS_W - 4 &&
                cy >= SYS_Y + 3 && cy < SYS_Y + 15)
            {
                gui_sysinfo_open = 0;
                needs_redraw = 1;
            }

            // Launcher item click
            if (gui_launcher_open)
            {
                int lx = LAUNCHER_X;
                int ly = TASKBAR_Y - LAUNCHER_H - 4;
                for (int i = 0; i < LAUNCHER_ITEMS; i++)
                {
                    int iy = ly + 24 + i * 28;
                    if (cx >= lx + 4 && cx < lx + LAUNCHER_W - 4 &&
                        cy >= iy && cy < iy + 24)
                    {
                        launcher_sel = i;
                        gui_launcher_open = 0;
                        if (i == 0)
                            gui_term_open = 1;
                        if (i == 1)
                            gui_sysinfo_open = 1;
                        if (i == 2)
                            gui_sysinfo_open = 1;
                        if (i == 3)
                        {
                            for (int j = 0; j < 60; j++)
                                terminal_putchar('\n');
                            outb(0xA1, 0xFF);
                            return;
                        }
                        needs_redraw = 1;
                    }
                }
            }
        }

        if (left != prev_left) needs_redraw = 1;
        prev_left = left;

        if (cx != prev_mx || cy != prev_my)
        {
            gui_erase_cursor();
            gui_draw_cursor(cx, cy);
            prev_mx = cx;
            prev_my = cy;
        }

        if (needs_redraw)
        {
            gui_erase_cursor();
            gui_draw_desktop();
            gui_draw_taskbar(cx, cy);
            if (gui_sysinfo_open)
                gui_draw_sysinfo();
            if (gui_term_open)
                gui_draw_term();
            if (gui_launcher_open)
                gui_draw_launcher();
            gui_draw_cursor(cx, cy);
        }
    }
}