/**
 * @file    page_file_system.c
 * @brief   LVGL POSIX 文件浏览器与文本/图片阅读页。
 * @author  suzumiya
 * @date    2025
 * @version 1.0.0
 * @license MIT
 */

#include "page_file_system.h"
#include <stdio.h>
#include <string.h>

#define FONT_PATH "/opt/lvgl/fonts/NotoSerifCJKsc-Regular.otf"
#define FONT_SIZE 24

static lv_font_t* g_chinese_font = NULL;
static bool       g_ft_inited    = false;

lv_file_struct_t lv_file;

typedef struct {
    char name[256];
    bool is_dir;
} lv_dir_entry_t;

uint16_t  lv_scan_files(const char* path, lv_obj_t* parent);
void      lv_del_list(lv_obj_t* parent);
void      lv_create_list(lv_obj_t* parent);
void      list_init(lv_obj_t* parent);
lv_obj_t* lv_create_page(lv_obj_t* parent);
char*     lv_path_joint(const char* current_path, const char* name);
void      lv_show_filetxt(void);
void      lv_show_imgbin(const char* path);
void      lv_btn_close_event(lv_event_t* event);
lv_fs_res_t lv_file_open_txt(const char* path);
lv_fs_res_t lv_file_load_page(uint32_t offset);

static bool lv_file_name_has_suffix(const char* name, const char* suffix)
{
    if (name == NULL || suffix == NULL) return false;
    size_t nlen = strlen(name);
    size_t slen = strlen(suffix);
    if (nlen < slen) return false;
    return strcmp(name + nlen - slen, suffix) == 0;
}

static int lv_dir_entry_cmp(const void* a, const void* b)
{
    const lv_dir_entry_t* ea = (const lv_dir_entry_t*)a;
    const lv_dir_entry_t* eb = (const lv_dir_entry_t*)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

static inline bool utf8_is_start(uint8_t c)
{
    return (c & 0xC0) != 0x80;
}

static uint32_t utf8_align_back(const char* buf, uint32_t pos)
{
    while (pos > 0) {
        uint8_t c = (uint8_t)buf[pos - 1];

        if ((c & 0x80) == 0) {
            return pos;
        }

        uint32_t start = pos - 1;
        while (start > 0 && (((uint8_t)buf[start] & 0xC0) == 0x80)) {
            start--;
        }

        uint8_t lead = (uint8_t)buf[start];
        uint32_t need = 0;

        if ((lead & 0xE0) == 0xC0)      need = 2;
        else if ((lead & 0xF0) == 0xE0) need = 3;
        else if ((lead & 0xF8) == 0xF0) need = 4;
        else                            return start;

        if (start + need <= pos) {
            return pos;
        }

        pos = start;
    }

    return 0;
}

static void font_init(void)
{
    if (g_chinese_font != NULL) return;

    if (!g_ft_inited) {
        if (!lv_freetype_init(0, 0, 512 * 1024)) {
            printf("[FONT] lv_freetype_init failed\n");
            return;
        }
        g_ft_inited = true;
    }

    lv_ft_info_t info;
    memset(&info, 0, sizeof(info));
    info.name     = FONT_PATH;
    info.weight   = FONT_SIZE;
    info.style    = FT_FONT_STYLE_NORMAL;
    info.mem      = NULL;
    info.mem_size = 0;

    if (!lv_ft_font_init(&info)) {
        printf("[FONT] lv_ft_font_init failed: %s\n", FONT_PATH);
        return;
    }

    g_chinese_font = info.font;
    printf("[FONT] loaded %s size=%d, ptr=%p\n",
           FONT_PATH, FONT_SIZE, (void*)g_chinese_font);
}

static void lvgl_to_real(const char* lvgl_path, char* real_path, size_t size)
{
    const char* rest = lvgl_path;

    if (strncmp(lvgl_path, LV_FS_DRIVE, 2) == 0) {
        rest = lvgl_path + 2;
    }

    while (*rest == '/') {
        rest++;
    }

    if (strcmp(SD_MOUNT_PATH, "/") == 0) {
        if (*rest == '\0') snprintf(real_path, size, "/");
        else               snprintf(real_path, size, "/%s", rest);
    } else {
        if (*rest == '\0') snprintf(real_path, size, "%s", SD_MOUNT_PATH);
        else               snprintf(real_path, size, "%s/%s", SD_MOUNT_PATH, rest);
    }
}

char* lv_path_joint(const char* current_path, const char* name)
{
    if (current_path == NULL || name == NULL) {
        lv_file.pname[0] = '\0';
        return lv_file.pname;
    }

    size_t len = strlen(current_path);

    if (len == 0) {
        snprintf(lv_file.pname, sizeof(lv_file.pname), "/%s", name);
    } else if (current_path[len - 1] == '/') {
        snprintf(lv_file.pname, sizeof(lv_file.pname), "%s%s", current_path, name);
    } else {
        snprintf(lv_file.pname, sizeof(lv_file.pname), "%s/%s", current_path, name);
    }

    return lv_file.pname;
}

lv_fs_res_t lv_file_open_txt(const char* path)
{
    if (lv_file.page_buf != NULL) {
        lv_mem_free(lv_file.page_buf);
        lv_file.page_buf = NULL;
    }

    strncpy(lv_file.txt_path, path, sizeof(lv_file.txt_path) - 1);
    lv_file.txt_path[sizeof(lv_file.txt_path) - 1] = '\0';

    lv_fs_file_t fd;
    lv_fs_res_t  res = lv_fs_open(&fd, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        printf("[TXT] open %s failed, res=%d\n", path, res);
        return res;
    }

    lv_file.file_size = 0;
    lv_fs_seek(&fd, 0, LV_FS_SEEK_END);
    lv_fs_tell(&fd, &lv_file.file_size);
    lv_fs_close(&fd);

    if (lv_file.file_size == 0) {
        printf("[TXT] file is empty\n");
        return LV_FS_RES_UNKNOWN;
    }

    lv_file.page_buf = (char*)lv_mem_alloc(TXT_PAGE_SIZE + 1);
    if (lv_file.page_buf == NULL) {
        printf("[TXT] alloc %d bytes failed\n", TXT_PAGE_SIZE + 1);
        return LV_FS_RES_OUT_OF_MEM;
    }

    lv_file.page_offset = 0;
    lv_file.page_end    = 0;

    printf("[TXT] opened %s, size=%u\n", path, lv_file.file_size);
    return lv_file_load_page(0);
}

lv_fs_res_t lv_file_load_page(uint32_t offset)
{
    if (lv_file.page_buf == NULL) return LV_FS_RES_UNKNOWN;
    if (offset >= lv_file.file_size) return LV_FS_RES_UNKNOWN;

    lv_fs_file_t fd;
    lv_fs_res_t  res = lv_fs_open(&fd, lv_file.txt_path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        printf("[TXT] reopen failed, res=%d\n", res);
        return res;
    }

    lv_fs_seek(&fd, offset, LV_FS_SEEK_SET);

    uint32_t rsize = 0;
    res = lv_fs_read(&fd, lv_file.page_buf, TXT_PAGE_SIZE, &rsize);
    lv_fs_close(&fd);

    if (res != LV_FS_RES_OK) {
        printf("[TXT] read failed, res=%d\n", res);
        return res;
    }

    uint32_t real_size = utf8_align_back(lv_file.page_buf, rsize);
    lv_file.page_buf[real_size] = '\0';

    lv_file.page_offset = offset;
    lv_file.page_end    = offset + real_size;

    printf("[TXT] page: offset=%u end=%u size=%u\n",
           lv_file.page_offset, lv_file.page_end, real_size);

    return LV_FS_RES_OK;
}

static void txt_update_page_info(void)
{
    if (lv_file.lv_page_info == NULL) return;

    uint32_t total = (lv_file.file_size + TXT_PAGE_SIZE - 1) / TXT_PAGE_SIZE;
    uint32_t cur   = lv_file.page_offset / TXT_PAGE_SIZE + 1;

    if (cur > total) cur = total;

    lv_label_set_text_fmt(lv_file.lv_page_info, "%u/%u", cur, total);
}

static void lv_btn_txt_page_event(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t* obj = lv_event_get_target(event);

    if (code != LV_EVENT_CLICKED) return;

    bool is_next = (obj == lv_file.lv_next_page_btn);
    uint32_t new_offset;

    if (is_next) {
        if (lv_file.page_end >= lv_file.file_size) return;
        new_offset = lv_file.page_end;
    } else {
        if (lv_file.page_offset == 0) return;
        new_offset = (lv_file.page_offset > TXT_PAGE_SIZE)
                     ? (lv_file.page_offset - TXT_PAGE_SIZE)
                     : 0;
    }

    if (lv_file_load_page(new_offset) != LV_FS_RES_OK) return;

    if (lv_file.lv_txt_label != NULL) {
        lv_label_set_text(lv_file.lv_txt_label, lv_file.page_buf);
        lv_obj_update_layout(lv_file.lv_txt_label);
    }

    txt_update_page_info();

    if (lv_file.lv_file_cont != NULL) {
        lv_obj_scroll_to_y(lv_file.lv_file_cont, 0, LV_ANIM_OFF);
    }
}

void lv_btn_close_event(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code != LV_EVENT_RELEASED && code != LV_EVENT_CLICKED) return;

    if (lv_file.lv_image_read != NULL) {
        lv_obj_del(lv_file.lv_image_read);
        lv_file.lv_image_read = NULL;
    }

    if (lv_file.lv_page_cont != NULL) {
        lv_obj_del(lv_file.lv_page_cont);
        lv_file.lv_page_cont = NULL;
    }

    lv_file.lv_file_cont     = NULL;
    lv_file.lv_txt_label     = NULL;
    lv_file.lv_page_info     = NULL;
    lv_file.lv_prev_page_btn = NULL;
    lv_file.lv_next_page_btn = NULL;
    lv_file.lv_return_page   = NULL;

    if (lv_file.page_buf != NULL) {
        lv_mem_free(lv_file.page_buf);
        lv_file.page_buf = NULL;
    }

    lv_file.page_offset = 0;
    lv_file.page_end    = 0;
    lv_file.file_size   = 0;
    lv_file.txt_path[0] = '\0';
}

lv_obj_t* lv_create_page(lv_obj_t* parent)
{
    lv_file.lv_page_cont = lv_obj_create(parent);
    lv_obj_set_size(lv_file.lv_page_cont, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_radius(lv_file.lv_page_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lv_file.lv_page_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_file.lv_page_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_file.lv_page_cont, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_file.lv_page_cont, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_clear_flag(lv_file.lv_page_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* top_bar = lv_obj_create(lv_file.lv_page_cont);
    lv_obj_set_size(top_bar, LCD_WIDTH, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_color(top_bar, lv_palette_main(LV_PALETTE_BLUE_GREY),
                              LV_STATE_DEFAULT);
    lv_obj_set_style_radius(top_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(top_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(top_bar, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(top_bar, 0, LV_STATE_DEFAULT);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* back_lbl = lv_label_create(top_bar);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(back_lbl, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Return");
    lv_obj_align(back_lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_flag(back_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_lbl, lv_btn_close_event, LV_EVENT_ALL, NULL);

    lv_file.lv_prev_page_btn = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lv_file.lv_prev_page_btn, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_file.lv_prev_page_btn, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(lv_file.lv_prev_page_btn, LV_SYMBOL_LEFT);
    lv_obj_align(lv_file.lv_prev_page_btn, LV_ALIGN_CENTER, -60, 0);
    lv_obj_add_flag(lv_file.lv_prev_page_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_file.lv_prev_page_btn, lv_btn_txt_page_event,
                        LV_EVENT_CLICKED, NULL);

    lv_file.lv_page_info = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lv_file.lv_page_info, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_file.lv_page_info, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(lv_file.lv_page_info, "1/1");
    lv_obj_align(lv_file.lv_page_info, LV_ALIGN_CENTER, 0, 0);

    lv_file.lv_next_page_btn = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lv_file.lv_next_page_btn, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_file.lv_next_page_btn, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(lv_file.lv_next_page_btn, LV_SYMBOL_RIGHT);
    lv_obj_align(lv_file.lv_next_page_btn, LV_ALIGN_CENTER, 60, 0);
    lv_obj_add_flag(lv_file.lv_next_page_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_file.lv_next_page_btn, lv_btn_txt_page_event,
                        LV_EVENT_CLICKED, NULL);

    lv_obj_t* time_lbl = lv_label_create(top_bar);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(time_lbl, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(time_lbl, "12:00");
    lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_file.lv_file_cont = lv_obj_create(lv_file.lv_page_cont);
    lv_obj_set_size(lv_file.lv_file_cont, LCD_WIDTH, LCD_HEIGHT - TOP_BAR_HEIGHT);
    lv_obj_align(lv_file.lv_file_cont, LV_ALIGN_TOP_MID, 0, TOP_BAR_HEIGHT);
    lv_obj_set_style_radius(lv_file.lv_file_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_file.lv_file_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_file.lv_file_cont, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_file.lv_file_cont, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lv_file.lv_file_cont, 10, LV_STATE_DEFAULT);
    lv_obj_set_scroll_dir(lv_file.lv_file_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(lv_file.lv_file_cont, LV_SCROLLBAR_MODE_AUTO);

    return lv_file.lv_page_cont;
}

void lv_show_filetxt(void)
{
    font_init();

    if (lv_file.page_buf == NULL) {
        printf("[TXT] page_buf is NULL\n");
        return;
    }

    lv_obj_t* label = lv_label_create(lv_file.lv_file_cont);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LCD_WIDTH - 30);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_STATE_DEFAULT);

    if (g_chinese_font != NULL) {
        lv_obj_set_style_text_font(label, g_chinese_font, LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    }

    lv_label_set_text(label, lv_file.page_buf);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_update_layout(label);

    lv_file.lv_txt_label = label;
    txt_update_page_info();

    printf("[TXT] label w=%d h=%d, page_buf len=%u\n",
           lv_obj_get_width(label), lv_obj_get_height(label),
           lv_file.page_end - lv_file.page_offset);
}

static bool is_valid_lvgl_bin(const char* path)
{
    lv_fs_file_t fd;
    lv_fs_res_t  res;
    uint32_t     br = 0;
    uint8_t      header[4];

    res = lv_fs_open(&fd, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) return false;

    res = lv_fs_read(&fd, header, 4, &br);
    lv_fs_close(&fd);

    if (res != LV_FS_RES_OK || br != 4) return false;
    if ((header[0] & 0xE0) != 0) return false;
    if ((header[0] & 0x1F) > 30) return false;

    return true;
}

void lv_show_imgbin(const char* path)
{
    if (!is_valid_lvgl_bin(path)) {
        lv_obj_t* label = lv_label_create(lv_file.lv_file_cont);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED),
                                    LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, LCD_WIDTH - 40);
        lv_label_set_text(label,
            "Invalid .bin file.\n\n"
            "Please use the LVGL image converter\n"
            "to generate the .bin file.");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    lv_file.lv_image_read = lv_img_create(lv_file.lv_file_cont);
    lv_img_set_src(lv_file.lv_image_read, path);
    lv_obj_align(lv_file.lv_image_read, LV_ALIGN_CENTER, 0, 0);
}

static void lv_list_btn_event(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t* obj = lv_event_get_target(event);

    if (code != LV_EVENT_CLICKED) return;

    for (int i = 0; i < lv_file.list_file_number; i++) {
        if (obj != lv_file.list_btn[i]) continue;

        const char* file_name = lv_list_get_btn_text(lv_file.list, lv_file.list_btn[i]);
        strncpy(lv_file.lv_pname, file_name, sizeof(lv_file.lv_pname) - 1);
        lv_file.lv_pname[sizeof(lv_file.lv_pname) - 1] = '\0';

        if (lv_file.btn_is_dir[i]) {
            if (lv_file.lv_prev_file_flag < LIST_SIZE) {
                strncpy(lv_file.lv_prev_file[lv_file.lv_prev_file_flag],
                        lv_file.lv_path,
                        sizeof(lv_file.lv_prev_file[0]) - 1);
                lv_file.lv_prev_file[lv_file.lv_prev_file_flag]
                    [sizeof(lv_file.lv_prev_file[0]) - 1] = '\0';
                lv_file.lv_prev_file_flag++;
            }

            char* new_path = lv_path_joint(lv_file.lv_path, lv_file.lv_pname);
            strncpy(lv_file.lv_path, new_path, sizeof(lv_file.lv_path) - 1);
            lv_file.lv_path[sizeof(lv_file.lv_path) - 1] = '\0';

            lv_del_list(lv_file.list);
            lv_scan_files(lv_file.lv_path, lv_scr_act());
        }
        else if (lv_file_name_has_suffix(lv_file.lv_pname, ".txt")) {
            char* full_path = lv_path_joint(lv_file.lv_path, lv_file.lv_pname);
            if (lv_file_open_txt(full_path) == LV_FS_RES_OK) {
                lv_file.lv_return_page = lv_create_page(lv_scr_act());
                lv_show_filetxt();
            }
        }
        else if (lv_file_name_has_suffix(lv_file.lv_pname, ".bin")) {
            char* full_path = lv_path_joint(lv_file.lv_path, lv_file.lv_pname);
            lv_file.lv_return_page = lv_create_page(lv_scr_act());
            lv_show_imgbin(full_path);
        }

        return;
    }
}

uint16_t lv_scan_files(const char* path, lv_obj_t* parent)
{
    char real_path[512];
    lvgl_to_real(path, real_path, sizeof(real_path));

    DIR* dir = opendir(real_path);
    if (dir == NULL) {
        perror("opendir");
        return 1;
    }

    static lv_dir_entry_t entries[LIST_SIZE];
    int entry_count = 0;

    struct dirent* de;
    while ((de = readdir(dir)) != NULL && entry_count < LIST_SIZE) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, "proc") == 0 ||
            strcmp(de->d_name, "sys")  == 0 ||
            strcmp(de->d_name, "dev")  == 0) continue;

        int is_dir = 0;

        if (de->d_type == DT_DIR) {
            is_dir = 1;
        } else if (de->d_type == DT_UNKNOWN) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", real_path, de->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
        }

        strncpy(entries[entry_count].name, de->d_name,
                sizeof(entries[entry_count].name) - 1);
        entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';
        entries[entry_count].is_dir = (is_dir != 0);
        entry_count++;
    }
    closedir(dir);

    qsort(entries, entry_count, sizeof(entries[0]), lv_dir_entry_cmp);

    memset(lv_file.list_btn,   0, sizeof(lv_file.list_btn));
    memset(lv_file.btn_is_dir, 0, sizeof(lv_file.btn_is_dir));
    lv_file.list_file_number = 0;
    lv_create_list(parent);

    if (strcmp(lv_file.lv_path, LV_FS_DRIVE) != 0) {
        lv_file.list_btn[0] = lv_list_add_btn(lv_file.list, LV_SYMBOL_DIRECTORY, "..");
        lv_obj_set_style_img_recolor(lv_file.list_btn[0], lv_color_hex(0xFFD700), 0);
        lv_obj_set_style_img_recolor_opa(lv_file.list_btn[0], LV_OPA_COVER, 0);
        lv_obj_add_event_cb(lv_file.list_btn[0], lv_list_btn_event, LV_EVENT_ALL, NULL);
        lv_file.btn_is_dir[0] = 1;
        lv_file.list_file_number = 1;
    }

    for (int i = 0; i < entry_count && lv_file.list_file_number < LIST_SIZE; i++) {
        uint16_t idx = lv_file.list_file_number;

        if (entries[i].is_dir) {
            lv_file.list_btn[idx] =
                lv_list_add_btn(lv_file.list, LV_SYMBOL_DIRECTORY, entries[i].name);
            lv_obj_set_style_img_recolor(lv_file.list_btn[idx], lv_color_hex(0xFFD700), 0);
            lv_obj_set_style_img_recolor_opa(lv_file.list_btn[idx], LV_OPA_COVER, 0);
            lv_file.btn_is_dir[idx] = 1;
        } else {
            const void* icon = LV_SYMBOL_FILE;

            if (lv_file_name_has_suffix(entries[i].name, ".png")  ||
                lv_file_name_has_suffix(entries[i].name, ".jpg")  ||
                lv_file_name_has_suffix(entries[i].name, ".jpeg") ||
                lv_file_name_has_suffix(entries[i].name, ".bmp")  ||
                lv_file_name_has_suffix(entries[i].name, ".gif")  ||
                lv_file_name_has_suffix(entries[i].name, ".avi")) {
                icon = LV_SYMBOL_IMAGE;
            }

            lv_file.list_btn[idx] =
                lv_list_add_btn(lv_file.list, icon, entries[i].name);
            lv_obj_set_style_img_recolor(lv_file.list_btn[idx], lv_color_hex(0x87CEFA), 0);
            lv_obj_set_style_img_recolor_opa(lv_file.list_btn[idx], LV_OPA_COVER, 0);
            lv_file.btn_is_dir[idx] = 0;
        }

        lv_obj_set_style_pad_left(lv_file.list_btn[idx], 5, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(lv_file.list_btn[idx], 5, LV_STATE_DEFAULT);
        lv_obj_add_event_cb(lv_file.list_btn[idx], lv_list_btn_event,
                            LV_EVENT_ALL, NULL);
        lv_file.list_file_number++;
    }

    return 0;
}

void lv_del_list(lv_obj_t* parent)
{
    if (parent != NULL) lv_obj_del(parent);
    lv_file.list = NULL;
}

void lv_animation(lv_obj_t* parent)
{
    lv_anim_del(parent, NULL);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, parent);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, LCD_WIDTH, 0);
    lv_anim_set_time(&a, 300);
    lv_anim_start(&a);
}

void lv_create_list(lv_obj_t* parent)
{
    lv_file.list = lv_list_create(parent);
    lv_animation(lv_file.list);
    lv_obj_set_size(lv_file.list, LCD_WIDTH,
                    LCD_HEIGHT - lv_font_montserrat_24.line_height * 2 - 60);
    lv_obj_set_style_pad_top(lv_file.list, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(lv_file.list, 10, LV_PART_MAIN);
    lv_obj_align_to(lv_file.list, lv_file.lv_page_obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    if (g_chinese_font != NULL) {
        lv_obj_set_style_text_font(lv_file.list, g_chinese_font, LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_font(lv_file.list, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    }

    lv_obj_set_style_radius(lv_file.list, 0, LV_STATE_DEFAULT);
}

void lv_page_tile(lv_obj_t* parent)
{
    lv_file.lv_page_obj = lv_obj_create(parent);
    lv_obj_set_size(lv_file.lv_page_obj, LCD_WIDTH,
                    lv_font_montserrat_24.line_height + 50);
    lv_obj_set_style_bg_color(lv_file.lv_page_obj,
                              lv_palette_main(LV_PALETTE_GREY), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lv_file.lv_page_obj, 0, LV_STATE_DEFAULT);
    lv_obj_align_to(lv_file.lv_page_obj, parent, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* lbl = lv_label_create(lv_file.lv_page_obj);
    lv_label_set_text(lbl, "File Management System");
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align_to(lbl, lv_file.lv_page_obj, LV_ALIGN_CENTER, 0, 0);
}

void lv_back_btn_event_handler(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t* obj = lv_event_get_target(event);

    if (code != LV_EVENT_CLICKED) return;

    if (obj == lv_file.lv_back_btn) {
        lv_file.lv_prev_file_flag = 0;
        strncpy(lv_file.lv_path, LV_FS_DRIVE, sizeof(lv_file.lv_path) - 1);
        lv_file.lv_path[sizeof(lv_file.lv_path) - 1] = '\0';
        lv_del_list(lv_file.list);
        lv_scan_files(lv_file.lv_path, lv_scr_act());
    }
    else if (obj == lv_file.lv_prev_btn) {
        if (lv_file.lv_prev_file_flag > 0) {
            lv_file.lv_prev_file_flag--;
            strncpy(lv_file.lv_path,
                    lv_file.lv_prev_file[lv_file.lv_prev_file_flag],
                    sizeof(lv_file.lv_path) - 1);
            lv_file.lv_path[sizeof(lv_file.lv_path) - 1] = '\0';
            lv_del_list(lv_file.list);
            lv_scan_files(lv_file.lv_path, lv_scr_act());
        }
    }
}

void lv_fs_win_create(lv_obj_t* parent)
{
    lv_file.lv_back_btn = lv_label_create(parent);
    lv_obj_set_style_text_font(lv_file.lv_back_btn, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(lv_file.lv_back_btn, "Menu");
    lv_obj_align_to(lv_file.lv_back_btn, parent, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_flag(lv_file.lv_back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_file.lv_back_btn, lv_back_btn_event_handler, LV_EVENT_ALL, NULL);

    lv_file.lv_prev_btn = lv_label_create(parent);
    lv_obj_set_style_text_font(lv_file.lv_prev_btn, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(lv_file.lv_prev_btn, "Return");
    lv_obj_align_to(lv_file.lv_prev_btn, parent, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_flag(lv_file.lv_prev_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lv_file.lv_prev_btn, lv_back_btn_event_handler, LV_EVENT_ALL, NULL);
}

void lv_page_back(lv_obj_t* parent)
{
    lv_file.lv_back_obj = lv_obj_create(parent);
    lv_obj_set_size(lv_file.lv_back_obj, LCD_WIDTH,
                    lv_font_montserrat_24.line_height + 10);
    lv_obj_set_style_bg_color(lv_file.lv_back_obj,
                              lv_palette_main(LV_PALETTE_GREY), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lv_file.lv_back_obj, 0, LV_STATE_DEFAULT);
    lv_obj_align_to(lv_file.lv_back_obj, parent, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_fs_win_create(lv_file.lv_back_obj);
}

void list_init(lv_obj_t* parent)
{
    strncpy(lv_file.lv_path, LV_FS_DRIVE, sizeof(lv_file.lv_path) - 1);
    lv_file.lv_path[sizeof(lv_file.lv_path) - 1] = '\0';

    lv_file.lv_prev_file_flag = 0;
    lv_file.list_file_number  = 0;
    lv_file.page_buf          = NULL;
    lv_file.txt_path[0]       = '\0';

    memset(lv_file.lv_prev_file, 0, sizeof(lv_file.lv_prev_file));
    memset(lv_file.btn_is_dir,   0, sizeof(lv_file.btn_is_dir));

    lv_scan_files(lv_file.lv_path, parent);
}

void lv_file_demo(void)
{
    memset(&lv_file, 0, sizeof(lv_file));

    font_init();
    lv_page_tile(lv_scr_act());
    lv_page_back(lv_scr_act());
    list_init(lv_scr_act());
}
