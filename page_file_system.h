/**
 * @file    page_file_system.h
 * @brief   LVGL POSIX 文件浏览器与文本/图片阅读页。
 * @author  suzumiya
 * @date    2025
 * @version 1.0.0
 * @license MIT
 *
 * @note    目标平台：ALIENTEK I.MX6U MINI，LVGL v8.2.0。
 *          支持目录浏览、UTF-8 文本阅读、LVGL 图片 bin 显示。
 */
#ifndef __PAGE_FILE_SYSTEM_H
#define __PAGE_FILE_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "lvgl/lvgl.h"

/* 可配置项 */
#define LIST_SIZE        300
#define TXT_PAGE_SIZE    (2 * 1024)
#define TOP_BAR_HEIGHT   50

#define SD_MOUNT_PATH    "/"
#define LV_FS_DRIVE      "S:"

#define LCD_WIDTH        lv_disp_get_hor_res(NULL)
#define LCD_HEIGHT       lv_disp_get_ver_res(NULL)

typedef struct
{
    /* 主页面 */
    lv_obj_t* list;
    lv_obj_t* lv_page_obj;
    lv_obj_t* list_btn[LIST_SIZE];
    uint8_t   btn_is_dir[LIST_SIZE];
    uint16_t  list_file_number;

    char lv_path[512];
    char lv_pname[256];
    char pname[512];

    int  lv_prev_file_flag;
    char lv_prev_file[LIST_SIZE][512];

    lv_obj_t* lv_back_obj;
    lv_obj_t* lv_prev_btn;
    lv_obj_t* lv_back_btn;

    /* 阅读页 */
    lv_obj_t* lv_page_cont;
    lv_obj_t* lv_file_cont;
    lv_obj_t* lv_image_read;
    lv_obj_t* lv_return_page;

    /* 文本阅读状态 */
    char      txt_path[512];
    char*     page_buf;
    uint32_t  file_size;
    uint32_t  page_offset;
    uint32_t  page_end;

    /* 阅读页 UI */
    lv_obj_t* lv_txt_label;
    lv_obj_t* lv_page_info;
    lv_obj_t* lv_prev_page_btn;
    lv_obj_t* lv_next_page_btn;
} lv_file_struct_t;

void lv_file_demo(void);

#endif /* __PAGE_FILE_SYSTEM_H */
