#pragma once
#include <stdint.h>
#include "mouse.h"
#include "io.h"
#include "ramdisk.h"

extern uint32_t *fb;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint64_t nexus_total_ram;

void terminal_putchar(char c);
void terminal_write(const char *str);

extern volatile char auth_key;
extern volatile int auth_key_ready;

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
#define COL_WIN_TIT_AK 0x3a5a8a
#define COL_WIN_BD 0x3a3a3a
#define COL_RESIZE 0x555555

#define TASKBAR_H 32
#define CURSOR_W 12
#define CURSOR_H 20
#define TITLEBAR_H 18
#define RESIZE_HIT 8
#define WIN_MIN_W 150
#define WIN_MIN_H 80
#define WIN_FILES 2

static void gui_pixel(int x, int y, uint32_t col)
{
    if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height)
        return;
    fb[y * (fb_pitch / 4) + x] = col;
}

static void gui_rect(int x, int y, int w, int h, uint32_t col)
{
    for (int row = 0; row < h; row++)
        for (int c = 0; c < w; c++)
            gui_pixel(x + c, y + row, col);
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
        buf[i++] = '0';
    else
    {
        while (n > 0)
        {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    for (int a = 0, b = i - 1; a < b; a++, b--)
    {
        char t = buf[a];
        buf[a] = buf[b];
        buf[b] = t;
    }
    buf[i] = '\0';
    gui_text(x, y, buf, col);
}

static int gui_prev_cx = -1, gui_prev_cy = -1;
static uint32_t gui_cursor_save[CURSOR_W * CURSOR_H];

static void gui_cursor_save_bg(int cx, int cy)
{
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            int sx = cx + col, sy = cy + row;
            gui_cursor_save[row * CURSOR_W + col] =
                (sx >= 0 && sy >= 0 && (uint32_t)sx < fb_width && (uint32_t)sy < fb_height)
                    ? fb[sy * (fb_pitch / 4) + sx]
                    : 0;
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

typedef struct
{
    int open, x, y, w, h;
    int dragging, resizing;
    int drag_ox, drag_oy;
    int resize_ox, resize_oy;
    int resize_orig_x, resize_orig_y, resize_orig_w, resize_orig_h;
    int active;
    const char *title;
} gui_win_t;

#define MAX_WINDOWS 4
static gui_win_t gui_windows[MAX_WINDOWS];
static int gui_active_win = -1;

static void win_init(int id, const char *title, int x, int y, int w, int h)
{
    gui_windows[id].open = 0;
    gui_windows[id].x = x;
    gui_windows[id].y = y;
    gui_windows[id].w = w;
    gui_windows[id].h = h;
    gui_windows[id].title = title;
    gui_windows[id].dragging = 0;
    gui_windows[id].resizing = 0;
    gui_windows[id].active = 0;
}

static void win_clamp(gui_win_t *w)
{
    int ty = (int)fb_height - TASKBAR_H;
    if (w->w < WIN_MIN_W)
        w->w = WIN_MIN_W;
    if (w->h < WIN_MIN_H)
        w->h = WIN_MIN_H;
    if (w->x < 0)
        w->x = 0;
    if (w->y < 0)
        w->y = 0;
    if (w->x + w->w > (int)fb_width)
        w->x = (int)fb_width - w->w;
    if (w->y + w->h > ty)
        w->y = ty - w->h;
}

static int win_resize_edge(gui_win_t *w, int mx, int my)
{
    if (!w->open)
        return 0;
    if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h)
        return 0;
    int mask = 0;
    if (mx < w->x + RESIZE_HIT)
        mask |= 1;
    if (mx >= w->x + w->w - RESIZE_HIT)
        mask |= 2;
    if (my < w->y + RESIZE_HIT)
        mask |= 4;
    if (my >= w->y + w->h - RESIZE_HIT)
        mask |= 8;
    return mask;
}

static int win_in_titlebar(gui_win_t *w, int mx, int my)
{
    return mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + TITLEBAR_H;
}

static int win_close_hit(gui_win_t *w, int mx, int my)
{
    return mx >= w->x + w->w - 20 && mx < w->x + w->w - 4 && my >= w->y + 3 && my < w->y + 15;
}

static void win_draw_chrome(gui_win_t *w)
{
    uint32_t tc = w->active ? COL_WIN_TIT_AK : COL_WIN_TIT;
    gui_rect(w->x, w->y, w->w, w->h, COL_WIN_BG);
    gui_rect(w->x, w->y, w->w, TITLEBAR_H, tc);
    gui_rect_outline(w->x, w->y, w->w, w->h, COL_WIN_BD);
    gui_text(w->x + 8, w->y + 5, w->title, w->active ? COL_TEXT : COL_TEXT_DIM);
    gui_rect(w->x + w->w - 20, w->y + 3, 16, 12, 0x992222);
    gui_text(w->x + w->w - 16, w->y + 5, "X", COL_TEXT);
    for (int i = 0; i < 3; i++)
    {
        gui_pixel(w->x + w->w - 4 - i * 3, w->y + w->h - 2, COL_RESIZE);
        gui_pixel(w->x + w->w - 2, w->y + w->h - 4 - i * 3, COL_RESIZE);
    }
}

static int win_mouse_down(int id, int cx, int cy)
{
    gui_win_t *w = &gui_windows[id];
    if (!w->open)
        return 0;
    if (cx < w->x || cx >= w->x + w->w || cy < w->y || cy >= w->y + w->h)
        return 0;
    if (gui_active_win != id)
    {
        if (gui_active_win >= 0)
            gui_windows[gui_active_win].active = 0;
        gui_active_win = id;
        w->active = 1;
    }
    if (win_close_hit(w, cx, cy))
    {
        w->open = 0;
        gui_active_win = -1;
        return 1;
    }
    int edge = win_resize_edge(w, cx, cy);
    if (edge && !win_in_titlebar(w, cx, cy))
    {
        w->resizing = edge;
        w->resize_ox = cx;
        w->resize_oy = cy;
        w->resize_orig_x = w->x;
        w->resize_orig_y = w->y;
        w->resize_orig_w = w->w;
        w->resize_orig_h = w->h;
        return 1;
    }
    if (win_in_titlebar(w, cx, cy))
    {
        w->dragging = 1;
        w->drag_ox = cx - w->x;
        w->drag_oy = cy - w->y;
        return 1;
    }
    return 1;
}

static int win_mouse_drag(int id, int cx, int cy)
{
    gui_win_t *w = &gui_windows[id];
    if (!w->open)
        return 0;
    if (w->dragging)
    {
        w->x = cx - w->drag_ox;
        w->y = cy - w->drag_oy;
        win_clamp(w);
        return 1;
    }
    if (w->resizing)
    {
        int dx = cx - w->resize_ox, dy = cy - w->resize_oy;
        if (w->resizing & 2)
            w->w = w->resize_orig_w + dx;
        if (w->resizing & 8)
            w->h = w->resize_orig_h + dy;
        if (w->resizing & 1)
        {
            w->x = w->resize_orig_x + dx;
            w->w = w->resize_orig_w - dx;
        }
        if (w->resizing & 4)
        {
            w->y = w->resize_orig_y + dy;
            w->h = w->resize_orig_h - dy;
        }
        win_clamp(w);
        return 1;
    }
    return 0;
}

static void win_mouse_up(int id)
{
    gui_windows[id].dragging = 0;
    gui_windows[id].resizing = 0;
}

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
    gui_rect(b->x, TASKBAR_Y + 6, b->w, TASKBAR_H - 12, hovered ? COL_BTN_HOV : COL_BTN);
    gui_rect_outline(b->x, TASKBAR_Y + 6, b->w, TASKBAR_H - 12, COL_BTN_BD);
    gui_text_centered(b->x, TASKBAR_Y + 12, b->w, b->label, COL_TEXT);
}

static int gui_btn_hit(int idx, int mx, int my)
{
    gui_btn_t *b = &gui_btns[idx];
    return mx >= b->x && mx < b->x + b->w && my >= TASKBAR_Y + 6 && my < TASKBAR_Y + TASKBAR_H - 6;
}

static int gui_launcher_open = 0;
#define LAUNCHER_W 200
#define LAUNCHER_H 160
#define LAUNCHER_X 8
#define LAUNCHER_ITEMS 4
static const char *launcher_items[LAUNCHER_ITEMS] = {"TERMINAL", "SYSINFO", "FILES", "CLOSE GUI"};
static int launcher_sel = 0;

static void gui_draw_launcher(void)
{
    int lx = LAUNCHER_X, ly = TASKBAR_Y - LAUNCHER_H - 4;
    gui_rect(lx, ly, LAUNCHER_W, LAUNCHER_H, COL_WIN_BG);
    gui_rect_outline(lx, ly, LAUNCHER_W, LAUNCHER_H, COL_WIN_BD);
    gui_text(lx + 8, ly + 8, "APPLICATIONS", COL_TEXT_DIM);
    for (int i = 0; i < LAUNCHER_ITEMS; i++)
    {
        int iy = ly + 24 + i * 28;
        gui_rect(lx + 4, iy, LAUNCHER_W - 8, 24, (i == launcher_sel) ? COL_BTN_HOV : COL_WIN_BG);
        gui_text(lx + 12, iy + 8, launcher_items[i], COL_TEXT);
    }
}

#define WIN_TERM 0
#define WIN_SYSINFO 1
#define TERM_ROWS 24
#define TERM_COLS 60

static char term_lines[TERM_ROWS][TERM_COLS + 1];
static int term_row = 0, term_col = 0;
static char term_input[TERM_COLS];
static int term_input_len = 0;

static void gui_term_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c <= TERM_COLS; c++)
            term_lines[r][c] = 0;
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
            for (int r = 0; r < TERM_ROWS - 1; r++)
                for (int c2 = 0; c2 <= TERM_COLS; c2++)
                    term_lines[r][c2] = term_lines[r + 1][c2];
            for (int c2 = 0; c2 <= TERM_COLS; c2++)
                term_lines[TERM_ROWS - 1][c2] = 0;
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
    gui_win_t *w = &gui_windows[WIN_TERM];
    win_draw_chrome(w);
    int cx = w->x + 2, cy = w->y + TITLEBAR_H, cw = w->w - 4, ch = w->h - TITLEBAR_H - 2;
    gui_rect(cx, cy, cw, ch, 0x0d1117);
    int rows_vis = ch / 9;
    if (rows_vis > TERM_ROWS)
        rows_vis = TERM_ROWS;
    for (int r = 0; r < rows_vis; r++)
        if (term_lines[r][0])
            gui_text(cx + 4, cy + 2 + r * 9, term_lines[r], COL_GREEN);
    int iy = cy + 2 + term_row * 9;
    if (iy < cy + ch - 9)
    {
        gui_text(cx + 4, iy, "$ ", COL_GREEN);
        gui_text(cx + 20, iy, term_input, COL_GREEN);
        gui_rect(cx + 20 + term_input_len * 8, iy, 6, 8, COL_GREEN);
    }
}

static void gui_draw_sysinfo(void)
{
    gui_win_t *w = &gui_windows[WIN_SYSINFO];
    win_draw_chrome(w);
    int tx = w->x + 16, ty = w->y + TITLEBAR_H + 10;
    gui_text(tx, ty, "OS:    NEXUSOS V0.4.0-alpha", COL_TEXT);
    gui_text(tx, ty + 18, "ARCH:  X86_64", COL_BLUE);
    gui_text(tx, ty + 36, "BOOT:  LIMINE V7", COL_TEXT);
    uint32_t ram_mb = (uint32_t)(nexus_total_ram / (1024 * 1024));
    gui_text(tx, ty + 54, "RAM:   ", COL_TEXT_DIM);
    gui_int(tx + 56, ty + 54, ram_mb, COL_BLUE);
    gui_text(tx + 56 + 48, ty + 54, " MB", COL_BLUE);
    gui_text(tx, ty + 72, "FS:    RAMDISK", COL_TEXT);
    gui_text(tx, ty + 90, "BY:    RONNIE & ROMEO", COL_TEXT_DIM);
}

#define FILES_ITEM_H 20
#define FILES_MAX_VIS 16

static int files_cwd = 0;
static int files_sel = -1;
static int files_scroll = 0;

static void gui_draw_files(void)
{
    gui_win_t *w = &gui_windows[WIN_FILES];
    win_draw_chrome(w);

    int cx = w->x + 2, cy = w->y + TITLEBAR_H;
    int cw = w->w - 4, ch = w->h - TITLEBAR_H - 2;
    gui_rect(cx, cy, cw, ch, 0x1e1e1e);

    // Path bar
    gui_rect(cx, cy, cw, 16, 0x2a2a2a);
    gui_rect_outline(cx, cy, cw, 16, COL_WIN_BD);

    // build path string by walking up to root
    char parts[16][RD_MAX_NAME];
    int depth = 0, node = files_cwd;
    while (node != 0 && depth < 16)
    {
        int i = 0;
        while (rd_nodes[node].name[i] && i < RD_MAX_NAME - 1)
            parts[depth][i] = rd_nodes[node].name[i++];
        parts[depth][i] = '\0';
        depth++;
        node = rd_nodes[node].parent;
    }
    char path[256];
    int pi = 0;
    path[pi++] = '/';
    for (int i = depth - 1; i >= 0 && pi < 250; i--)
    {
        int j = 0;
        while (parts[i][j] && pi < 250)
            path[pi++] = parts[i][j++];
        if (i > 0)
            path[pi++] = '/';
    }
    path[pi] = '\0';
    gui_text(cx + 4, cy + 4, path, COL_TEXT_DIM);

    // list area
    int list_y = cy + 18;
    int list_h = ch - 38; // 18 path bar + 20 status bar
    gui_rect(cx, list_y, cw, list_h, 0x1e1e1e);

    rd_node_t *dir = &rd_nodes[files_cwd];
    int total = dir->num_children;
    int show_back = (files_cwd != 0) ? 1 : 0;
    int total_ent = total + show_back;
    int visible = list_h / FILES_ITEM_H;
    if (visible > FILES_MAX_VIS)
        visible = FILES_MAX_VIS;
    if (files_scroll > total_ent - visible)
        files_scroll = total_ent - visible;
    if (files_scroll < 0)
        files_scroll = 0;

    for (int i = 0; i < visible; i++)
    {
        int ei = i + files_scroll;
        if (ei >= total_ent)
            break;
        int ey = list_y + i * FILES_ITEM_H;

        if (ei == files_sel)
            gui_rect(cx, ey, cw, FILES_ITEM_H, 0x2a4a6a);
        for (int xx = cx; xx < cx + cw; xx++)
            gui_pixel(xx, ey + FILES_ITEM_H - 1, 0x2a2a2a);

        if (show_back && ei == 0)
        {
            gui_text(cx + 6, ey + 6, "^", COL_BLUE);
            gui_text(cx + 24, ey + 6, "..", COL_TEXT_DIM);
        }
        else
        {
            int ci = ei - show_back;
            if (ci < 0 || ci >= dir->num_children)
                break;
            rd_node_t *child = &rd_nodes[dir->children[ci]];
            if (child->type == RD_DIR)
            {
                gui_text(cx + 6, ey + 6, "D", COL_BLUE);
                gui_text(cx + 24, ey + 6, child->name, COL_BLUE);
                gui_text(cx + cw - 40, ey + 6, "DIR", COL_TEXT_DIM);
            }
            else
            {
                gui_text(cx + 6, ey + 6, "F", COL_TEXT_DIM);
                gui_text(cx + 24, ey + 6, child->name, COL_TEXT);
                gui_int(cx + cw - 56, ey + 6, child->size, COL_TEXT_DIM);
                gui_text(cx + cw - 56 + gui_strlen("999999") * 8, ey + 6, "B", COL_TEXT_DIM);
            }
        }
    }

    // scrollbar
    if (total_ent > visible && visible > 0)
    {
        int sb_x = cx + cw - 4, sb_h = list_h;
        gui_rect(sb_x, list_y, 4, sb_h, 0x2a2a2a);
        int th = sb_h * visible / total_ent;
        if (th < 8)
            th = 8;
        int ty2 = list_y + (sb_h - th) * files_scroll / (total_ent > visible ? total_ent - visible : 1);
        gui_rect(sb_x, ty2, 4, th, COL_TEXT_DIM);
    }

    // Status bar
    int sy = cy + ch - 20;
    gui_rect(cx, sy, cw, 20, 0x2a2a2a);
    gui_rect_outline(cx, sy, cw, 20, COL_WIN_BD);
    if (files_sel >= 0 && files_sel < total_ent)
    {
        if (show_back && files_sel == 0)
        {
            gui_text(cx + 4, sy + 6, "Parent directory", COL_TEXT_DIM);
        }
        else
        {
            int ci = files_sel - show_back;
            if (ci >= 0 && ci < dir->num_children)
            {
                rd_node_t *sel = &rd_nodes[dir->children[ci]];
                gui_text(cx + 4, sy + 6, sel->name, sel->type == RD_DIR ? COL_BLUE : COL_TEXT);
            }
        }
    }
    else
    {
        gui_text(cx + 4, sy + 6, "Items:", COL_TEXT_DIM);
        gui_int(cx + 56, sy + 6, (uint32_t)total, COL_TEXT_DIM);
    }
}

static void gui_draw_desktop(void)
{
    gui_rect(0, 0, fb_width, fb_height - TASKBAR_H, COL_DESKTOP);
    for (uint32_t x = 0; x < fb_width; x += 40)
        for (uint32_t y = 0; y < fb_height - TASKBAR_H; y += 40)
            gui_pixel(x, y, 0x222222);
    gui_text_centered(0, (fb_height - TASKBAR_H) / 2 - 4, fb_width, "NEXUSOS DESKTOP", COL_TEXT_HINT);
    gui_text_centered(0, (fb_height - TASKBAR_H) / 2 + 12, fb_width, "ESC = RETURN TO SHELL", 0x333333);
}

static void gui_draw_taskbar(int mx, int my)
{
    gui_rect(0, TASKBAR_Y, fb_width, TASKBAR_H, COL_TASKBAR);
    for (uint32_t x = 0; x < fb_width; x++)
        gui_pixel(x, TASKBAR_Y, COL_TASKBAR_BD);
    for (int i = 0; i < gui_num_btns; i++)
        gui_draw_btn(i, gui_btn_hit(i, mx, my));
    gui_text_centered(200, TASKBAR_Y + 12, (int)fb_width - 400, "CPU --  RAM --", COL_TEXT_HINT);
    gui_text((int)fb_width - 72, TASKBAR_Y + 12, "NEXUSOS", COL_TEXT_DIM);
}

static void gui_run(void)
{
    mouse_init((int)fb_width, (int)fb_height);
    outb(0x21, 0xF9);
    outb(0xA1, 0xEF);

    win_init(WIN_TERM, "TERMINAL", 60, 60, 480, 280);
    win_init(WIN_SYSINFO, "SYSTEM INFO", 120, 100, 340, 200);
    win_init(WIN_FILES, "FILES", 80, 50, 300, 340);

    gui_num_btns = 0;
    gui_btn_add(8, 80, "  APPS");
    gui_btn_add(96, 80, "TERMINAL");
    gui_btn_add(184, 70, "SYSINFO");
    gui_btn_add(262, 60, "FILES");

    gui_term_clear();
    gui_term_write("NEXUSOS TERMINAL\n$ ");

    gui_draw_desktop();
    gui_draw_taskbar(mouse_x, mouse_y);
    gui_draw_cursor(mouse_x, mouse_y);

    int prev_mx = mouse_x, prev_my = mouse_y;
    int prev_left = 0, needs_redraw = 0;
    int dragging_win = -1;

    while (1)
    {
        needs_redraw = 0;

        if (auth_key_ready)
        {
            char c = auth_key;
            auth_key_ready = 0;
            if (c == 27)
            {
                for (int i = 0; i < 60; i++)
                    terminal_putchar('\n');
                outb(0xA1, 0xFF);
                return;
            }
            if (gui_launcher_open)
            {
                if (c == '\n')
                {
                    gui_launcher_open = 0;
                    if (launcher_sel == 0)
                        gui_windows[WIN_TERM].open = 1;
                    if (launcher_sel == 1)
                        gui_windows[WIN_SYSINFO].open = 1;
                    if (launcher_sel == 3)
                    {
                        for (int i = 0; i < 60; i++)
                            terminal_putchar('\n');
                        outb(0xA1, 0xFF);
                        return;
                    }
                    needs_redraw = 1;
                }
            }
            else if (gui_windows[WIN_TERM].open && gui_active_win == WIN_TERM)
            {
                if (c == '\n')
                {
                    gui_term_putchar('\n');
                    term_input[term_input_len] = '\0';
                    if (term_input[0] == 'c' && term_input[1] == 'l' && term_input[2] == 's')
                        gui_term_clear();
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

        int cx = mouse_x, cy = mouse_y, left = mouse_left;

        if (left && !prev_left)
        {
            int consumed = 0;
            for (int i = MAX_WINDOWS - 1; i >= 0; i--)
            {
                if (!gui_windows[i].open)
                    continue;
                if (win_mouse_down(i, cx, cy))
                {
                    dragging_win = i;
                    consumed = 1;
                    needs_redraw = 1;
                    break;
                }
            }
            if (!consumed)
            {
                // Files navigation on click
                if (gui_windows[WIN_FILES].open)
                {
                    gui_win_t *fw = &gui_windows[WIN_FILES];
                    int fx = fw->x + 2, fy = fw->y + TITLEBAR_H + 18;
                    int fh = fw->h - TITLEBAR_H - 38;
                    if (cx >= fx && cx < fx + fw->w - 4 &&
                        cy >= fy && cy < fy + fh)
                    {
                        int clicked = (cy - fy) / FILES_ITEM_H + files_scroll;
                        rd_node_t *fdir = &rd_nodes[files_cwd];
                        int fback = (files_cwd != 0) ? 1 : 0;
                        int ftotal = fdir->num_children + fback;
                        if (clicked < ftotal)
                        {
                            if (clicked == files_sel)
                            {
                                // Second click = navigate into dir
                                if (fback && clicked == 0)
                                {
                                    files_cwd = rd_nodes[files_cwd].parent;
                                    files_sel = -1;
                                    files_scroll = 0;
                                }
                                else
                                {
                                    int fci = clicked - fback;
                                    if (fci >= 0 && fci < fdir->num_children)
                                    {
                                        int fnidx = fdir->children[fci];
                                        if (rd_nodes[fnidx].type == RD_DIR)
                                        {
                                            files_cwd = fnidx;
                                            files_sel = -1;
                                            files_scroll = 0;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                files_sel = clicked;
                            }
                            needs_redraw = 1;
                        }
                    }
                }

                for (int i = 0; i < gui_num_btns; i++)
                {
                    if (gui_btn_hit(i, cx, cy))
                    {
                        if (i == 0)
                            gui_launcher_open = !gui_launcher_open;
                        if (i == 1)
                        {
                            gui_windows[WIN_TERM].open = !gui_windows[WIN_TERM].open;
                            gui_launcher_open = 0;
                        }
                        if (i == 2)
                        {
                            gui_windows[WIN_SYSINFO].open = !gui_windows[WIN_SYSINFO].open;
                            gui_launcher_open = 0;
                        }
                        if (i == 3)
                        {
                            gui_windows[WIN_FILES].open = !gui_windows[WIN_FILES].open;
                            gui_launcher_open = 0;
                        }
                        needs_redraw = 1;
                    }
                }
                if (gui_launcher_open)
                {
                    int lx = LAUNCHER_X, ly = TASKBAR_Y - LAUNCHER_H - 4;
                    for (int i = 0; i < LAUNCHER_ITEMS; i++)
                    {
                        int iy = ly + 24 + i * 28;
                        if (cx >= lx + 4 && cx < lx + LAUNCHER_W - 4 && cy >= iy && cy < iy + 24)
                        {
                            launcher_sel = i;
                            gui_launcher_open = 0;
                            if (i == 0)
                                gui_windows[WIN_TERM].open = 1;
                            if (i == 1)
                                gui_windows[WIN_SYSINFO].open = 1;
                            if (i == 2)
                                gui_windows[WIN_FILES].open = 1;
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
        }

        if (left && dragging_win >= 0)
        {
            if (win_mouse_drag(dragging_win, cx, cy))
                needs_redraw = 1;
        }

        if (!left && prev_left && dragging_win >= 0)
        {
            win_mouse_up(dragging_win);
            dragging_win = -1;
            needs_redraw = 1;
        }

        if (left != prev_left)
            needs_redraw = 1;
        prev_left = left;

        if (cx != prev_mx || cy != prev_my)
        {
            gui_erase_cursor();
            if (dragging_win >= 0)
            {
                // redraw fully during drag for clean window movement
                needs_redraw = 1;
            }
            else
            {
                gui_draw_cursor(cx, cy);
            }
            prev_mx = cx;
            prev_my = cy;
        }

        if (needs_redraw)
        {
            gui_erase_cursor();
            gui_draw_desktop();
            gui_draw_taskbar(cx, cy);
            if (gui_windows[WIN_SYSINFO].open)
                gui_draw_sysinfo();
            if (gui_windows[WIN_TERM].open)
                gui_draw_term();
            if (gui_windows[WIN_FILES].open)
                gui_draw_files();
            if (gui_launcher_open)
                gui_draw_launcher();
            gui_draw_cursor(cx, cy);
        }
    }
}