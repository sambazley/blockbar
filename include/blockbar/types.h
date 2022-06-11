#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <cairo.h>

typedef uint8_t color [4];

enum pos {
    LEFT,
    RIGHT,
    CENTER,
    SIDES
};

enum setting_type {
    INT,
    BOOL,
    STR,
    COL,
    POS,
};

union value {
    int INT;
    int BOOL;
    char *STR;
    color COL;
    enum pos POS;
};

struct setting {
    char *name;
    char *desc;
    enum setting_type type;
    union value def, val;
};

struct bar_settings {
    struct setting height;
    struct setting marginvert;
    struct setting marginhoriz;
    struct setting xoffset;
    struct setting radius;
    struct setting padding;
    struct setting background;
    struct setting foreground;
    struct setting font;
    struct setting position;
    struct setting divwidth;
    struct setting divheight;
    struct setting divvertmargin;
    struct setting divcolor;
    struct setting borderwidth;
    struct setting bordercolor;
    struct setting traydiv;
    struct setting traypadding;
    struct setting trayiconsize;
    struct setting traybar;
    struct setting trayside;
};

struct properties {
    struct setting module;
    struct setting exec;
    struct setting pos;
    struct setting interval;
    struct setting padding;
    struct setting paddingleft;
    struct setting paddingright;
    struct setting nodiv;
};

struct block_data {
    int rendered;
    char *exec_data;
};

struct block {
    int id;
    int eachmon;
    int task;

    int *width;
    int *x;
    cairo_surface_t **sfc;

    struct properties properties;
    struct block_data *data;
};

struct click {
    int button;
    int x;
    int bar;
};

#define MFLAG_NO_EXEC (1<<0)

enum module_type {
    BLOCK,
    RENDER,
};

struct module_data {
    char *name;

    enum module_type type;
    long flags;

    struct setting *settings;
    int setting_count;

    int interval;
};

struct module {
    void *dl;
    char *path;
    int in_config;
    int task;

    struct module_data data;

    cairo_surface_t **sfc;
    int zindex;
    int timePassed;
};

#endif /* SETTINGS_H */
