#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef uint8_t color [4];

enum Mode {
    LEGACY,
    SUBBLOCK
};

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
    MODE
};

union Value {
    int INT;
    int BOOL;
    char *STR;
    color COL;
    enum Pos POS;
    enum Mode MODE;
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
    struct Setting radius;
    struct Setting padding;
    struct Setting background;
    struct Setting foreground;
    struct Setting font;
    struct Setting shortlabels;
    struct Setting position;
    struct Setting divwidth;
    struct Setting divheight;
    struct Setting divvertmargin;
    struct Setting divcolor;
    struct Setting traydiv;
    struct Setting traypadding;
    struct Setting trayiconsize;
    struct Setting traybar;
    struct Setting trayside;
};

struct Properties {
    struct Setting mode;
    struct Setting label;
    struct Setting exec;
    struct Setting pos;
    struct Setting interval;
    struct Setting padding;
    struct Setting paddingleft;
    struct Setting paddingright;
    struct Setting nodiv;
};

struct LegacyData {
    int rendered;
    char *execData;
};

struct SubblockData {
    int rendered;
    char *execData;
    int *widths;
    int subblockCount;
};

struct Block {
    int id;
    int eachmon;
    int timePassed;
    int *width;
    int *x;

    struct Properties properties;

    union {
        union {
            struct LegacyData legacy;
            struct SubblockData subblock;
        } type;

        struct {
            union {
                struct LegacyData legacy;
                struct SubblockData subblock;
            } type;
        } *mon;
    } data;
};

#endif /* SETTINGS_H */
