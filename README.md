# LVGL 文件系统页面 / File System Page

**作者：** suzumiya  
**版本：** 1.0.0  
**日期：** 2025  
**许可证：** MIT  
**目标平台：** ALIENTEK I.MX6U MINI / LVGL v8.2.0 / Linux POSIX 文件系统

---

## 1. 项目简介

本项目是一个运行在正点原子 I.MX6U 开发板上的 LVGL 文件浏览器页面。它通过 LVGL 的 POSIX 文件系统驱动访问开发板文件系统，提供目录浏览、文本阅读和 LVGL 图片 bin 显示功能。

主要功能：

1. 读取开发板文件系统，查找文件夹和文件。
2. 文件夹优先、名称升序排序。
3. 使用 LVGL 列表显示目录内容。
4. 文件系统只读，不提供删除、重命名、写入操作。
5. 点击文件夹进入下一级目录。
6. 支持打开 UTF-8 编码的 `.txt` 文件。
7. 支持显示由 LVGL 图片转换工具生成的 `.bin` 图片文件。
8. 文本阅读使用 FreeType 加载 OTF 字体，支持中文显示。
9. 支持返回上一级目录和返回根目录。
10. 文本阅读支持上一页 / 下一页分页。

> 注意：当前实现只处理 `.txt` 和 `.bin`，其他文件会显示图标，但点击后不会打开。

---

## 2. 文件说明

| 文件 | 说明 |
|---|---|
| `page_file_system.c` | 文件浏览器、文本阅读、图片显示主实现 |
| `page_file_system.h` | 数据结构、配置宏、入口声明 |
| `README.md` | 移植、字体获取、使用说明 |

---

## 3. 依赖环境

- 开发板：正点原子 I.MX6U MINI 或类似 ARM Linux 开发板
- GUI：LVGL v8.2.0
- 字体引擎：FreeType
- 文件系统：POSIX / Linux 文件系统
- 显示：已适配 LVGL 的 LCD
- 输入：触摸屏或按键

详细移植教程和演示效果：
https://lcny35gtv2jn.feishu.cn/wiki/OXp1wYJVUiYME8k45fXcE1aOnlg?from=from_copylink

联系作者：1832839786@qq.com
