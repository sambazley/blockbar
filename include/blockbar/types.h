#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <cairo.h>

typedef uint8_t color [4];

enum Pos {
    LEFT,
    RIGHT,
    CENTER,
    SIDES
};

enum SettingType {
    INT,
    BOOL,
    STR,
    COL,
    POS,
};

union Value {
    int INT;
    int BOOL;
    char *STR;
    color COL;
    enum Pos POS;
};

struct Setting {
    char *name;
    char *desc;
    enum SettingType type;
    union Value def, val;
};

struct Settings {
    struct Setting height;
    struct Setting marginvert;
    struct Setting marginhoriz;
    struct Setting xoffset;
    struct Setting radius;
    struct Setting padding;
    struct Setting background;
    struct Setting foreground;
    struct Setting font;
    struct Setting position;
    struct Setting divwidth;
    struct Setting divheight;
    struct Setting divvertmargin;
    struct Setting divcolor;
    struct Setting borderwidth;
    struct Setting bordercolor;
    struct Setting traydiv;
    struct Setting traypadding;
    struct Setting trayiconsize;
    struct Setting traybar;
    struct Setting trayside;
};

struct Properties {
    struct Setting module;
    struct Setting exec;
    struct Setting pos;
    struct Setting interval;
    struct Setting padding;
    struct Setting paddingleft;
    struct Setting paddingright;
    struct Setting nodiv;
};

struct BlockData {
    int rendered;
    char *execData;
};

struct Block {
    int id;
    int eachmon;
    int timePassed;

    int *width;
    int *x;
    cairo_surface_t **sfc;

    struct Properties properties;

    struct BlockData *data;
};

struct Click {
    int button;
    int x;
    int bar;
};

#define MFLAG_NO_EXEC (1<<0)

struct ModuleData {
    char *name;

    long flags;

    struct Setting *settings;
    int settingCount;
};

struct Module {
    void *dl;
    char *path;
    int inConfig;
    struct ModuleData data;
};

#endif /* SETTINGS_H */
