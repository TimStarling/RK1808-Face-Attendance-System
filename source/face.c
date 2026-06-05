#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../rockx-sdk-rk1808-linux/include/rockx.h"
#include "DRMwrap.h"
#include "camera.h"

/*
 * 灞忓箷鍥哄畾涓?1024x600銆? * DRM 鏄惧瓨鎸?BGRA 4 瀛楄妭涓€涓儚绱犲啓鍏ワ紝鍥犳缁樺埗浣嶇疆閮戒互璇ュ垎杈ㄧ巼涓哄熀鍑嗐€? */
#define SCREEN_W 1024
#define SCREEN_H 600

/*
 * 棣栭〉涓変釜涓昏鎸夐挳鐨勮Е鎽稿尯鍩熴€? * 鍧愭爣鏉ヨ嚜 1.bmp 涓荤晫闈㈠浘鐗囷細
 * - 绠＄悊鍛樻ā寮忥細宸︿笂瑙掓寜閽? * - 浜鸿劯褰曞叆锛氬乏渚уぇ鍗＄墖
 * - 鑰冨嫟鎵撳崱锛氬彸渚уぇ鍗＄墖
 */
#define BTN_ADMIN_X1 0
#define BTN_ADMIN_Y1 0
#define BTN_ADMIN_X2 175
#define BTN_ADMIN_Y2 85

#define BTN_ENROLL_X1 180
#define BTN_ENROLL_Y1 170
#define BTN_ENROLL_X2 475
#define BTN_ENROLL_Y2 512

#define BTN_CHECK_X1 550
#define BTN_CHECK_Y1 170
#define BTN_CHECK_X2 845
#define BTN_CHECK_Y2 512

/*
 * 褰曞叆椤?鎵撳崱椤典笂鐨勫姩浣滄寜閽尯鍩熴€? * 鍦ㄥ綍鍏ラ〉浠ｈ〃鈥滃綍鍏ュ綋鍓嶄汉鑴糕€濓紝鍦ㄦ墦鍗￠〉浠ｈ〃鈥滃紑濮嬫墦鍗¤瘑鍒€濄€? */
#define BTN_ACTION_X1 700
#define BTN_ACTION_Y1 150
#define BTN_ACTION_X2 960
#define BTN_ACTION_Y2 460

/* 褰曞叆椤点€佹墦鍗￠〉鍙充笂瑙掕繑鍥炴寜閽殑鍖哄煙銆?*/
#define BTN_BACK_X1 735
#define BTN_BACK_Y1 45
#define BTN_BACK_X2 945
#define BTN_BACK_Y2 130

/*
 * RockX 浜鸿劯鐩镐技搴﹂槇鍊笺€? * 褰撳墠 RockX 鎺ュ彛杩斿洖鐨?similarity 瓒婂皬瓒婄浉浼硷紝鍥犳鎴愬姛鏉′欢涓猴細
 *     similarity > 0 && similarity <= FACE_MATCH_THRESHOLD
 */
#define FACE_MATCH_THRESHOLD 1.1f

/*
 * 鎽勫儚澶撮瑙堝尯鍩熴€? * 鎽勫儚澶磋緭鍑轰负 640x480锛屽綋鍓嶆樉绀哄湪灞忓箷 (0,60)-(640,540)锛屼笉鎷変几銆佷笉鍘嬬缉銆? */
#define PREVIEW_X 0
#define PREVIEW_Y 60
#define PREVIEW_W 640
#define PREVIEW_H 480

/*
 * 杩愯鏃剁洰褰曘€? * 绋嬪簭蹇呴』鍦?/class_work 涓嬭繍琛岋紝鍥犳杩欓噷閮戒娇鐢ㄧ浉瀵硅矾寰勩€? */
#define FACE_DIR "./face"
#define RECORD_DIR "./record"
#define ENROLL_RAW_BMP FACE_DIR "/enroll_preview.bmp"
#define ENROLL_BOX_BMP FACE_DIR "/enroll_preview_box.bmp"
#define CHECK_RAW_BMP FACE_DIR "/check_preview.bmp"
#define CHECK_BOX_BMP FACE_DIR "/check_preview_box.bmp"
#define KEEP_FACE_BMP FACE_DIR "/keep_face.bmp"
#define ENROLL_SUCCESS_BMP "./2_success.bmp"
#define ENROLL_FAIL_BMP "./2_fail.bmp"
#define CHECK_SUCCESS_BMP "./3_success.bmp"
#define CHECK_FAIL_BMP "./3_fail.bmp"
#define MAX_FACE_NAME_LEN 31
#define ADMIN_PAGE_SIZE 6

#define ADMIN_TAB_FACES 0
#define ADMIN_TAB_RECORDS 1

/* DRM銆佹憚鍍忓ご銆佽Е鎽歌澶囩殑鍏ㄥ眬鍙ユ焺銆?*/
static struct drmHandle g_drm;
static int g_drm_fd = -1;
static int g_video_fd = -1;
static int g_touch_fd = -1;
static volatile sig_atomic_t g_running = 1;

/*
 * 瀹炴椂棰勮鐢ㄧ殑 RGB 鍐呭瓨甯с€? * 浼樺寲鍓嶆瘡甯ч兘浼氫繚瀛?璇诲彇 BMP锛涚幇鍦ㄦ瘡甯х洿鎺ラ噰闆嗗埌璇ョ紦鍐插尯锛? * 鍐嶇洿鎺ョ敤浜?DRM 鏄剧ず鍜?RockX 浜鸿劯妫€娴嬨€? */
static unsigned char g_preview_rgb[640 * 480 * 3];

/* RockX 涓変釜妯″瀷鍙ユ焺锛氭娴嬨€佷汉鑴镐簲鐐瑰榻愩€佷汉鑴歌瘑鍒€?*/
static rockx_handle_t g_face_det_handle;
static rockx_handle_t g_face_5landmarks_handle;
static rockx_handle_t g_face_recognize_handle;

/* 璧勬簮鍒濆鍖栨爣蹇楋紝鐢ㄤ簬 cleanup() 涓垽鏂槸鍚﹂渶瑕侀噴鏀俱€?*/
static int g_drm_ready = 0;
static int g_face_det_ready = 0;
static int g_face_5landmarks_ready = 0;
static int g_face_recognize_ready = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

/* 鍒ゆ柇瑙︽懜鍧愭爣鏄惁钀藉湪鏌愪釜鐭╁舰鎸夐挳鍖哄煙鍐呫€?*/
static int in_rect(int x, int y, int x1, int y1, int x2, int y2) {
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

/*
 * 鍦?DRM 鏄惧瓨涓啓涓€涓儚绱犮€? * 娉ㄦ剰 DRM 鏄惧瓨浣跨敤鐨勬槸 B/G/R/Alpha 椤哄簭锛屽洜姝ゅ弬鏁颁篃鏄?b銆乬銆乺銆? */
static void draw_pixel(int x, int y, unsigned char b, unsigned char g, unsigned char r) {
    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL || x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return;
    }

    int pos = 4 * (SCREEN_W * y + x);
    screen[pos + 0] = b;
    screen[pos + 1] = g;
    screen[pos + 2] = r;
    screen[pos + 3] = 0;
}

/* 濉厖鐭╁舰鍖哄煙锛屽父鐢ㄤ簬鎸夐挳鑳屾櫙銆佹枃瀛楀簳鑹层€佹竻灞忓尯鍩熴€?*/
static void fill_rect(int x1, int y1, int x2, int y2,
                      unsigned char b, unsigned char g, unsigned char r) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            draw_pixel(x, y, b, g, r);
        }
    }
}

/* 缁樺埗鐭╁舰杈规锛涘疄鏃朵汉鑴告鍜屾寜閽竟妗嗛兘浣跨敤璇ュ嚱鏁般€?*/
static void draw_rect_border(int x1, int y1, int x2, int y2, int width,
                             unsigned char b, unsigned char g, unsigned char r) {
    for (int i = 0; i < width; i++) {
        for (int x = x1 + i; x <= x2 - i; x++) {
            draw_pixel(x, y1 + i, b, g, r);
            draw_pixel(x, y2 - i, b, g, r);
        }
        for (int y = y1 + i; y <= y2 - i; y++) {
            draw_pixel(x1 + i, y, b, g, r);
            draw_pixel(x2 - i, y, b, g, r);
        }
    }
}

/* Bresenham 鐢荤嚎鍑芥暟锛岀敤浜庣粯鍒惰繑鍥炵澶寸瓑绠€鍗曞浘褰€?*/
static void draw_line(int x1, int y1, int x2, int y2, int width,
                      unsigned char b, unsigned char g, unsigned char r) {
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        for (int oy = -width / 2; oy <= width / 2; oy++) {
            for (int ox = -width / 2; ox <= width / 2; ox++) {
                draw_pixel(x1 + ox, y1 + oy, b, g, r);
            }
        }
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/*
 * 鍦ㄥ綍鍏ラ〉鍜屾墦鍗￠〉鍙充笂瑙掔粯鍒惰繑鍥炴寜閽€? * 杩欎釜鎸夐挳涓嶆槸 BMP 鍥剧墖鐨勪竴閮ㄥ垎锛岃€屾槸绋嬪簭杩愯鏃剁洿鎺ョ敾鍒?DRM 鏄惧瓨涓€? */
static void draw_back_button(void) {
    const unsigned char blue_b = 210;
    const unsigned char blue_g = 73;
    const unsigned char blue_r = 38;
    const unsigned char gold_b = 66;
    const unsigned char gold_g = 184;
    const unsigned char gold_r = 225;

    fill_rect(BTN_BACK_X1, BTN_BACK_Y1, BTN_BACK_X2, BTN_BACK_Y2, 255, 255, 255);
    draw_rect_border(BTN_BACK_X1, BTN_BACK_Y1, BTN_BACK_X2, BTN_BACK_Y2, 5,
                     gold_b, gold_g, gold_r);
    draw_rect_border(BTN_BACK_X1 + 8, BTN_BACK_Y1 + 8, BTN_BACK_X2 - 8, BTN_BACK_Y2 - 8, 2,
                     blue_b, blue_g, blue_r);

    draw_line(BTN_BACK_X1 + 64, BTN_BACK_Y1 + 42, BTN_BACK_X1 + 94, BTN_BACK_Y1 + 22, 5,
              blue_b, blue_g, blue_r);
    draw_line(BTN_BACK_X1 + 64, BTN_BACK_Y1 + 42, BTN_BACK_X1 + 94, BTN_BACK_Y1 + 62, 5,
              blue_b, blue_g, blue_r);
    draw_line(BTN_BACK_X1 + 64, BTN_BACK_Y1 + 42, BTN_BACK_X2 - 42, BTN_BACK_Y1 + 42, 5,
	              blue_b, blue_g, blue_r);
}

/*
 * 5x7 鑻辨枃/鏁板瓧鐐归樀瀛楀簱銆? * 鐢ㄤ簬鏂囦欢鍚嶃€佺紪鍙枫€佽嫳鏂囬敭鐩樸€丠i,xxx 绛?ASCII 瀛楃鏄剧ず銆? */
static const unsigned char *get_glyph(char ch) {
    static const unsigned char glyphs[][7] = {
        {14, 17, 17, 31, 17, 17, 17}, // a
        {30, 17, 17, 30, 17, 17, 30}, // b
        {14, 17, 16, 16, 16, 17, 14}, // c
        {30, 17, 17, 17, 17, 17, 30}, // d
        {31, 16, 16, 30, 16, 16, 31}, // e
        {31, 16, 16, 30, 16, 16, 16}, // f
        {14, 17, 16, 23, 17, 17, 14}, // g
        {17, 17, 17, 31, 17, 17, 17}, // h
        {14, 4, 4, 4, 4, 4, 14},     // i
        {7, 2, 2, 2, 18, 18, 12},    // j
        {17, 18, 20, 24, 20, 18, 17}, // k
        {16, 16, 16, 16, 16, 16, 31}, // l
        {17, 27, 21, 21, 17, 17, 17}, // m
        {17, 25, 21, 19, 17, 17, 17}, // n
        {14, 17, 17, 17, 17, 17, 14}, // o
        {30, 17, 17, 30, 16, 16, 16}, // p
        {14, 17, 17, 17, 21, 18, 13}, // q
        {30, 17, 17, 30, 20, 18, 17}, // r
        {15, 16, 16, 14, 1, 1, 30},   // s
        {31, 4, 4, 4, 4, 4, 4},       // t
        {17, 17, 17, 17, 17, 17, 14}, // u
        {17, 17, 17, 17, 17, 10, 4},  // v
        {17, 17, 17, 21, 21, 21, 10}, // w
        {17, 17, 10, 4, 10, 17, 17},  // x
        {17, 17, 10, 4, 4, 4, 4},     // y
        {31, 1, 2, 4, 8, 16, 31},     // z
        {14, 17, 19, 21, 25, 17, 14}, // 0
        {4, 12, 4, 4, 4, 4, 14},      // 1
        {14, 17, 1, 2, 4, 8, 31},     // 2
        {30, 1, 1, 14, 1, 1, 30},     // 3
        {2, 6, 10, 18, 31, 2, 2},     // 4
        {31, 16, 30, 1, 1, 17, 14},   // 5
        {6, 8, 16, 30, 17, 17, 14},   // 6
        {31, 1, 2, 4, 8, 8, 8},       // 7
        {14, 17, 17, 14, 17, 17, 14}, // 8
        {14, 17, 17, 15, 1, 2, 12},   // 9
        {0, 0, 0, 0, 0, 0, 0},        // space
        {0, 0, 0, 31, 0, 0, 0}        // -
    };

    if (ch >= 'a' && ch <= 'z') {
        return glyphs[ch - 'a'];
    }
    if (ch >= 'A' && ch <= 'Z') {
        return glyphs[ch - 'A'];
    }
    if (ch >= '0' && ch <= '9') {
        return glyphs[26 + ch - '0'];
    }
    if (ch == ' ') {
        return glyphs[36];
    }
    if (ch == '-' || ch == '_') {
        return glyphs[37];
    }
    return glyphs[36];
}

/* 鎸夋寚瀹氬€嶆暟缁樺埗涓€涓?ASCII 瀛楃銆?*/
static void draw_char(int x, int y, char ch, int scale,
                      unsigned char b, unsigned char g, unsigned char r) {
    const unsigned char *glyph = get_glyph(ch);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph[row] & (1 << (4 - col))) {
                fill_rect(x + col * scale, y + row * scale,
                          x + (col + 1) * scale - 1, y + (row + 1) * scale - 1,
                          b, g, r);
            }
        }
    }
}

/* 缁樺埗 ASCII 瀛楃涓层€?*/
static void draw_text(int x, int y, const char *text, int scale,
                      unsigned char b, unsigned char g, unsigned char r) {
    int cursor = x;
    while (*text != '\0') {
        draw_char(cursor, y, *text, scale, b, g, r);
        cursor += 6 * scale;
        text++;
    }
}

/*
 * 涓枃鐐归樀瀛楀簱缁撴瀯銆? * code 涓?Unicode 缂栫爜锛況ows 涓?24x24 鐐归樀锛屾瘡涓?unsigned int 瀛樹竴琛屻€? */
typedef struct {
    unsigned int code;
    unsigned int rows[24];
} cn_glyph_t;

/*
 * 灏忓瀷涓枃鐐归樀瀛楀簱銆? * 鍙寘鍚湰椤圭洰鐣岄潰鍥哄畾鐢ㄥ瓧锛屼緥濡傦細绠＄悊鍛樸€佷汉鑴稿簱銆佹墦鍗¤褰曘€佸垹闄ゃ€佽繑鍥炪€佽ˉ鍗＄瓑銆? * 杩欐牱鍋氬彲浠ラ伩鍏嶄緷璧栨澘瀛愮郴缁熷瓧浣撱€? */
static const cn_glyph_t g_cn_glyphs[] = {
    {0x7BA1, {
        0x000000, 0x040300, 0x0C0300, 0x0FF7FC,
        0x198660, 0x318C60, 0x209C20, 0x3FFFFC,
        0x30000C, 0x30000C, 0x37FFCC, 0x0600C0,
        0x0600C0, 0x0600C0, 0x07FFC0, 0x060000,
        0x060000, 0x07FFF0, 0x060070, 0x060070,
        0x07FFF0, 0x060070, 0x000000, 0x000000
    } },
    {0x7406, {
        0x000000, 0x000000, 0x007FF8, 0x7FE318,
        0x0C6318, 0x0C6318, 0x0C7FF8, 0x0C6318,
        0x0C6318, 0x3FE318, 0x0C6318, 0x0C7FF8,
        0x0C0300, 0x0C0300, 0x0F8300, 0x0F3FF8,
        0x7C0300, 0x300300, 0x000300, 0x00FFFE,
        0x000000, 0x000000, 0x000000, 0x000000
    } },
    {0x5458, {
        0x000000, 0x000000, 0x03FFC0, 0x0300C0,
        0x0300C0, 0x0300C0, 0x03FFC0, 0x000000,
        0x000000, 0x07FFE0, 0x060060, 0x061860,
        0x061860, 0x061860, 0x061860, 0x061860,
        0x063060, 0x063060, 0x066E00, 0x00C3C0,
        0x078070, 0x3E0018, 0x000000, 0x000000
    } },
    {0x6A21, {
        0x000000, 0x000000, 0x0E1860, 0x0E1860,
        0x0EFFFC, 0x0E1860, 0x7F8060, 0x0E0000,
        0x0E7FF8, 0x0E7018, 0x0E7FF8, 0x1F7018,
        0x1FF018, 0x3FFFF8, 0x7E0300, 0x7E0300,
        0x6EFFFC, 0x0E0780, 0x0E0CC0, 0x0E1860,
        0x0E7038, 0x0EC00E, 0x000000, 0x000000
    } },
    {0x5F0F, {
        0x000000, 0x000000, 0x000600, 0x0006C0,
        0x0006F0, 0x000630, 0x000600, 0x3FFFFC,
        0x000600, 0x000600, 0x000600, 0x000300,
        0x1FFB00, 0x018300, 0x018300, 0x018300,
        0x018180, 0x018180, 0x01FCC4, 0x0FE0C6,
        0x3C006C, 0x00002C, 0x000018, 0x000000
    } },
    {0x9996, {
        0x000000, 0x0300C0, 0x0381C0, 0x01C180,
        0x3FFFFC, 0x001C00, 0x001800, 0x001800,
        0x07FFE0, 0x060060, 0x060060, 0x07FFE0,
        0x060060, 0x060060, 0x060060, 0x07FFE0,
        0x060060, 0x060060, 0x060060, 0x07FFE0,
        0x060060, 0x000000, 0x000000, 0x000000
    } },
    {0x9875, {
        0x000000, 0x000000, 0x3FFFFC, 0x001800,
        0x003800, 0x003000, 0x03FFE0, 0x030060,
        0x030060, 0x030060, 0x031860, 0x031860,
        0x031860, 0x031860, 0x031860, 0x033060,
        0x033E60, 0x036700, 0x00E1C0, 0x01C060,
        0x0F0030, 0x380010, 0x000000, 0x000000
    } },
    {0x5F55, {
        0x000000, 0x0FFFC0, 0x0000C0, 0x0000C0,
        0x0000C0, 0x03FFC0, 0x0000C0, 0x0001C0,
        0x3FFFFC, 0x001800, 0x0C1800, 0x061C10,
        0x031C70, 0x011BE0, 0x0079C0, 0x01D8C0,
        0x071830, 0x1C181C, 0x301806, 0x001800,
        0x007000, 0x000000, 0x000000, 0x000000
    } },
    {0x5165, {
        0x000000, 0x000000, 0x01FC00, 0x000C00,
        0x000C00, 0x000C00, 0x000C00, 0x000C00,
        0x000C00, 0x001C00, 0x001E00, 0x003600,
        0x003600, 0x006300, 0x00E180, 0x00C180,
        0x0180C0, 0x030060, 0x060030, 0x1C001C,
        0x30000C, 0x000000, 0x000000, 0x000000
    } },
    {0x8003, {
        0x000000, 0x000000, 0x003000, 0x003018,
        0x003038, 0x0FFFF0, 0x0031E0, 0x0031C0,
        0x003380, 0x3FFFFC, 0x001800, 0x002000,
        0x00C0F0, 0x07FFC0, 0x1F8000, 0x718000,
        0x038000, 0x03FFF0, 0x030060, 0x030060,
        0x000060, 0x001FC0, 0x000000, 0x000000
    } },
    {0x52E4, {
        0x000000, 0x000000, 0x0630C0, 0x0630C0,
        0x7FFEC0, 0x0630C0, 0x0630C0, 0x0003FC,
        0x3FFCCC, 0x318CCC, 0x318CCC, 0x318CCC,
        0x3FFCCC, 0x0180CC, 0x01808C, 0x3FFD8C,
        0x01818C, 0x01818C, 0x1FFD0C, 0x01830C,
        0x07FE18, 0x7FCC78, 0x000000, 0x000000
    } },
    {0x4EBA, {
        0x000000, 0x000000, 0x001800, 0x001800,
        0x001800, 0x001800, 0x001800, 0x001800,
        0x001800, 0x001800, 0x001800, 0x003800,
        0x003C00, 0x007C00, 0x006600, 0x00E300,
        0x01C300, 0x038180, 0x0700C0, 0x0E0070,
        0x180038, 0x30000C, 0x000000, 0x000000
    } },
    {0x8138, {
        0x000000, 0x000100, 0x1F8300, 0x198780,
        0x198780, 0x198EC0, 0x199CE0, 0x1FF870,
        0x19F03C, 0x19FFFC, 0x198000, 0x198618,
        0x19A718, 0x1FB330, 0x11BB30, 0x319BB0,
        0x319960, 0x319860, 0x7180C0, 0x61FFFC,
        0x6F0000, 0x000000, 0x000000, 0x000000
    } },
    {0x5E93, {
        0x000000, 0x001800, 0x001C00, 0x000C00,
        0x1FFFFC, 0x183000, 0x183000, 0x183000,
        0x1BFFF8, 0x186000, 0x18C600, 0x19C600,
        0x19FFF0, 0x180600, 0x180600, 0x180600,
        0x1FFFFC, 0x180600, 0x380600, 0x300600,
        0x600600, 0x600600, 0x000600, 0x000000
    } },
    {0x6253, {
        0x000000, 0x030000, 0x030000, 0x033FFE,
        0x0300C0, 0x0300C0, 0x3FE0C0, 0x0300C0,
        0x0300C0, 0x0300C0, 0x0300C0, 0x0320C0,
        0x03C0C0, 0x0700C0, 0x1F00C0, 0x3300C0,
        0x0300C0, 0x0300C0, 0x0300C0, 0x0300C0,
        0x0300C0, 0x1E0F80, 0x000000, 0x000000
    } },
    {0x5361, {
        0x000000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003FF8, 0x003000, 0x003000,
        0x003000, 0x003000, 0x3FFFFC, 0x003000,
        0x003000, 0x003700, 0x0031C0, 0x003060,
        0x003030, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003000, 0x000000, 0x000000
    } },
    {0x8BB0, {
        0x000000, 0x000000, 0x080000, 0x1C0000,
        0x0E3FF8, 0x070018, 0x020018, 0x000018,
        0x000018, 0x3E0018, 0x060018, 0x061FF8,
        0x061818, 0x061800, 0x061800, 0x061800,
        0x061800, 0x061804, 0x07D806, 0x07980C,
        0x06180C, 0x000FF8, 0x000000, 0x000000
    } },
    {0x6570, {
        0x000000, 0x000000, 0x030100, 0x1B1180,
        0x1F7300, 0x0F6300, 0x7FFBFE, 0x0F6318,
        0x1F7318, 0x3B3718, 0x331718, 0x070DB0,
        0x0619B0, 0x0609B0, 0x7FF0F0, 0x1C60E0,
        0x1CC0E0, 0x0FC1E0, 0x03C1B0, 0x07E718,
        0x1C2E0C, 0x700806, 0x000000, 0x000000
    } },
    {0x91CF, {
        0x000000, 0x000000, 0x07FFE0, 0x060060,
        0x07FFE0, 0x060060, 0x060060, 0x07FFE0,
        0x000000, 0x3FFFFC, 0x000000, 0x0FFFF0,
        0x0C1830, 0x0FFFF0, 0x0C1830, 0x0C1830,
        0x0FFFF0, 0x001800, 0x1FFFF8, 0x001800,
        0x7FFFFE, 0x000000, 0x000000, 0x000000
    } },
    {0x5220, {
        0x000000, 0x000018, 0x3F7E18, 0x336618,
        0x3366D8, 0x3366D8, 0x3366D8, 0x3366D8,
        0x3366D8, 0x3366D8, 0x7FFFD8, 0x3366D8,
        0x3366D8, 0x3366D8, 0x3366D8, 0x3366D8,
        0x3366D8, 0x3366D8, 0x33E618, 0x73C618,
        0x63C618, 0x6E9C78, 0x000000, 0x000000
    } },
    {0x9664, {
        0x000000, 0x000300, 0x000700, 0x1F8780,
        0x198F80, 0x199CC0, 0x1BB860, 0x1B7870,
        0x1FF03C, 0x1EFFFE, 0x1F0300, 0x1B0300,
        0x198300, 0x19FFFC, 0x198320, 0x198370,
        0x1F1338, 0x183B18, 0x18730C, 0x18E30E,
        0x198304, 0x180300, 0x181E00, 0x000000
    } },
    {0x8FD4, {
        0x000000, 0x100000, 0x387FFC, 0x186000,
        0x0C6000, 0x0E6000, 0x046000, 0x007FF8,
        0x006038, 0x7E6030, 0x067C30, 0x066770,
        0x0663E0, 0x0661E0, 0x06E1E0, 0x06C730,
        0x07DE18, 0x06B81C, 0x0F0008, 0x1BC000,
        0x30FFFE, 0x600000, 0x000000, 0x000000
    } },
    {0x56DE, {
        0x000000, 0x000000, 0x000000, 0x1FFFF8,
        0x180018, 0x180018, 0x180018, 0x19FF98,
        0x198398, 0x198398, 0x198398, 0x198398,
        0x198398, 0x198398, 0x19FF98, 0x180018,
        0x180018, 0x180018, 0x1FFFF8, 0x180018,
        0x180018, 0x000000, 0x000000, 0x000000
    } },
    {0x4E0A, {
        0x000000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003FF0, 0x003000, 0x003000,
        0x003000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003000, 0x003000, 0x003000,
        0x3FFFFC, 0x000000, 0x000000, 0x000000
    } },
    {0x4E00, {
        0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x1FFFFC, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000
    } },
    {0x4E0B, {
        0x000000, 0x000000, 0x3FFFFC, 0x003000,
        0x003000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003600, 0x003380, 0x0031C0,
        0x0030E0, 0x003030, 0x003030, 0x003000,
        0x003000, 0x003000, 0x003000, 0x003000,
        0x003000, 0x003000, 0x000000, 0x000000
    } },
    {0x6210, {
        0x000000, 0x000EC0, 0x000EF0, 0x000638,
        0x000618, 0x000600, 0x0FFFFC, 0x0C0600,
        0x0C0618, 0x0C0638, 0x0C0630, 0x0FF670,
        0x0C3360, 0x0C33E0, 0x0C33C0, 0x0C3380,
        0x186780, 0x19EFC4, 0x181CC6, 0x307864,
        0x70303C, 0x20001C, 0x000000, 0x000000
    } },
    {0x529F, {
        0x000000, 0x000000, 0x000300, 0x000300,
        0x000300, 0x3FE300, 0x030300, 0x030300,
        0x033FFC, 0x030318, 0x030318, 0x030318,
        0x030318, 0x033718, 0x03C618, 0x070618,
        0x1C0618, 0x300C18, 0x001818, 0x003038,
        0x00E3F0, 0x018000, 0x000000, 0x000000
    } },
    {0x5931, {
        0x000000, 0x000C00, 0x000C00, 0x010C00,
        0x030C00, 0x030C00, 0x03FFF0, 0x060C00,
        0x0E0C00, 0x0C0C00, 0x081C00, 0x001C00,
        0x1FFFFC, 0x001E00, 0x003600, 0x007300,
        0x006380, 0x00C180, 0x0180E0, 0x070070,
        0x1E0038, 0x38000C, 0x000000, 0x000000
    } },
    {0x8D25, {
        0x000000, 0x000300, 0x000300, 0x1FE300,
        0x186300, 0x1B6700, 0x1B67FC, 0x1B6630,
        0x1B6E30, 0x1B6E30, 0x1B7F30, 0x1B6B30,
        0x1B6360, 0x1B6160, 0x1B61E0, 0x1E61C0,
        0x0601C0, 0x0781E0, 0x0CE370, 0x1C6638,
        0x382C1C, 0x60180C, 0x000000, 0x000000
    } },
    {0x8865, {
        0x000000, 0x0C0000, 0x0E0180, 0x070180,
        0x030180, 0x000180, 0x3FC180, 0x01C180,
        0x01C1C0, 0x01A1E0, 0x03F1F0, 0x07E1B8,
        0x0FC19C, 0x1B618E, 0x333184, 0x632180,
        0x030180, 0x030180, 0x030180, 0x030180,
        0x030180, 0x030180, 0x030180, 0x000000
    } },
};
static const int g_cn_glyph_count = sizeof(g_cn_glyphs) / sizeof(g_cn_glyphs[0]);

/*
 * 璇诲彇 UTF-8 瀛楃涓蹭腑鐨勪笅涓€涓瓧绗︼紝骞惰繑鍥?Unicode 缂栫爜銆? * 涓枃瀛楃涓插湪婧愮爜涓互 UTF-8 瀛樺偍锛岀粯鍒跺墠闇€瑕佸厛瑙ｇ爜鍒?Unicode銆? */
static unsigned int utf8_next_code(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    if (s[0] < 0x80) {
        *p += 1;
        return s[0];
    }
    if ((s[0] & 0xE0) == 0xC0 && s[1] != 0) {
        *p += 2;
        return ((unsigned int)(s[0] & 0x1F) << 6) |
               (unsigned int)(s[1] & 0x3F);
    }
    if ((s[0] & 0xF0) == 0xE0 && s[1] != 0 && s[2] != 0) {
        *p += 3;
        return ((unsigned int)(s[0] & 0x0F) << 12) |
               ((unsigned int)(s[1] & 0x3F) << 6) |
               (unsigned int)(s[2] & 0x3F);
    }
    *p += 1;
    return '?';
}

/* 鏍规嵁 Unicode 缂栫爜鏌ユ壘瀵瑰簲涓枃鐐归樀銆?*/
static const cn_glyph_t *find_cn_glyph(unsigned int code) {
    for (int i = 0; i < g_cn_glyph_count; i++) {
        if (g_cn_glyphs[i].code == code) {
            return &g_cn_glyphs[i];
        }
    }
    return NULL;
}

/* 缁樺埗涓€涓?24x24 涓枃鐐归樀锛屽彲閫氳繃 scale 鏀惧ぇ銆?*/
static void draw_cn_glyph(int x, int y, const cn_glyph_t *glyph, int scale,
                          unsigned char b, unsigned char g, unsigned char r) {
    if (glyph == NULL) {
        return;
    }

    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 24; col++) {
            if (glyph->rows[row] & (1U << (23 - col))) {
                fill_rect(x + col * scale, y + row * scale,
                          x + (col + 1) * scale - 1, y + (row + 1) * scale - 1,
                          b, g, r);
            }
        }
    }
}

/*
 * 缁樺埗 UTF-8 瀛楃涓层€? * 涓枃璧?24x24 鐐归樀锛孉SCII 璧?5x7 鐐归樀锛屽洜姝ゅ彲浠ユ贩鍚堟樉绀猴細
 *     "浜鸿劯3 璁板綍5"
 */
static void draw_utf8_text(int x, int y, const char *text, int cn_scale, int ascii_scale,
                           unsigned char b, unsigned char g, unsigned char r) {
    int cursor = x;
    const char *p = text;

    while (*p != '\0') {
        unsigned int code = utf8_next_code(&p);
        if (code < 0x80) {
            draw_char(cursor, y + (24 * cn_scale - 7 * ascii_scale) / 2,
                      (char)code, ascii_scale, b, g, r);
            cursor += 6 * ascii_scale;
        } else {
            const cn_glyph_t *glyph = find_cn_glyph(code);
            if (glyph != NULL) {
                draw_cn_glyph(cursor, y, glyph, cn_scale, b, g, r);
            }
            cursor += 26 * cn_scale;
        }
    }
}

/* 璁＄畻 UTF-8 瀛楃涓茬粯鍒跺搴︼紝鏂逛究鍋氬眳涓樉绀恒€?*/
static int utf8_text_width(const char *text, int cn_scale, int ascii_scale) {
    int width = 0;
    const char *p = text;

    while (*p != '\0') {
        unsigned int code = utf8_next_code(&p);
        width += code < 0x80 ? 6 * ascii_scale : 26 * cn_scale;
    }

    return width;
}

static int read_all(int fd, void *buf, size_t size) {
    unsigned char *p = buf;
    size_t total = 0;

    while (total < size) {
        ssize_t n = read(fd, p + total, size - total);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }

    return 0;
}

/* 纭繚浜鸿劯搴撶洰褰曞瓨鍦細./face銆?*/
static int ensure_face_dir(void) {
    if (mkdir(FACE_DIR, 0777) == -1 && errno != EEXIST) {
        perror("mkdir face dir error");
        return -1;
    }

    return 0;
}

/* 纭繚鎵撳崱璁板綍鐩綍瀛樺湪锛?/record銆?*/
static int ensure_record_dir(void) {
    if (mkdir(RECORD_DIR, 0777) == -1 && errno != EEXIST) {
        perror("mkdir record dir error");
        return -1;
    }

    return 0;
}

/* 鏍规嵁 event 鍚嶇О鎵撳紑 /dev/input/eventX銆?*/
static int open_touch_event(const char *event_name) {
    char path[64];
    int n = snprintf(path, sizeof(path), "/dev/input/%s", event_name);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return -1;
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        printf("touch device selected: %s\n", path);
    }
    return fd;
}

/*
 * 鑷姩鏌ユ壘瑙︽懜璁惧銆? *
 * 涓嶅悓鏉垮瓙鐨勮Е鎽稿睆 event 缂栧彿鍙兘涓嶅悓锛屼笉鑳藉浐瀹氫娇鐢?event1/event2銆? * 杩欓噷璇诲彇 /proc/bus/input/devices锛岄€夋嫨甯?ABS 鍧愭爣鑳藉姏鐨勮緭鍏ヨ澶囷紝
 * 骞惰烦杩?camera銆乸wrkey銆乲eys 绛夐潪瑙︽懜璁惧銆? */
static int init_touch(void) {
    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (fp != NULL) {
        char line[256];
        char event_name[32] = {0};
        int has_abs = 0;
        int skip_device = 0;

        while (fgets(line, sizeof(line), fp) != NULL) {
            if (line[0] == '\n') {
                if (has_abs && !skip_device && event_name[0] != '\0') {
                    int fd = open_touch_event(event_name);
                    if (fd >= 0) {
                        fclose(fp);
                        return fd;
                    }
                }
                event_name[0] = '\0';
                has_abs = 0;
                skip_device = 0;
                continue;
            }

            if (strncmp(line, "N: Name=", 8) == 0) {
                if (strstr(line, "camera") != NULL ||
                    strstr(line, "Camera") != NULL ||
                    strstr(line, "pwrkey") != NULL ||
                    strstr(line, "keys") != NULL) {
                    skip_device = 1;
                }
            } else if (strncmp(line, "H: Handlers=", 12) == 0) {
                char *p = strstr(line, "event");
                if (p != NULL) {
                    sscanf(p, "%31s", event_name);
                }
            } else if (strncmp(line, "B: ABS=", 7) == 0) {
                has_abs = 1;
            }
        }

        if (has_abs && !skip_device && event_name[0] != '\0') {
            int fd = open_touch_event(event_name);
            if (fd >= 0) {
                fclose(fp);
                return fd;
            }
        }
        fclose(fp);
    }

    for (int i = 0; i < 10; i++) {
        char event_name[32];
        snprintf(event_name, sizeof(event_name), "event%d", i);
        int fd = open_touch_event(event_name);
        if (fd >= 0) {
            return fd;
        }
    }

    perror("open touch device error");
    return -1;
}

static void drain_touch_events(void);

/*
 * 闃诲绛夊緟涓€娆″畬鏁寸偣鍑汇€? *
 * 浼樺厛浣跨敤 BTN_TOUCH 鐨勬澗鎵嬩簨浠朵綔涓轰竴娆＄偣鍑伙紝閬垮厤鎸変笅杩囩▼涓噸澶嶈Е鍙戙€? * 瀵逛簬娌℃湁 BTN_TOUCH 鐨勮Е鎽稿睆锛屼娇鐢?SYN_REPORT 浣滀负鍏滃簳瑙﹀彂銆? */
static int wait_touch(int *out_x, int *out_y) {
    struct input_event ev;
    int x = -1;
    int y = -1;
    int has_xy = 0;
    int saw_btn_touch = 0;

    while (g_running) {
        ssize_t n = read(g_touch_fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            }
            return -1;
        }

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                x = ev.value;
                has_xy = 1;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                y = ev.value;
                has_xy = 1;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            saw_btn_touch = 1;
            if (ev.value != 0) {
                continue;
            }
            if (x >= 0 && y >= 0) {
                *out_x = x;
                *out_y = y;
                printf("touch x=%d y=%d\n", x, y);
                usleep(120000);
                drain_touch_events();
                return 1;
            }
        } else if (!saw_btn_touch && ev.type == EV_SYN && ev.code == SYN_REPORT && has_xy && x >= 0 && y >= 0) {
            *out_x = x;
            *out_y = y;
            printf("touch x=%d y=%d\n", x, y);
            usleep(120000);
            drain_touch_events();
            return 1;
        }
    }

    return 0;
}

/*
 * 娓呯┖瑙︽懜浜嬩欢闃熷垪銆? * 椤甸潰鍒囨崲鍓嶅悗璋冪敤璇ュ嚱鏁帮紝闃叉涓€娆¤Е鎽哥殑娈嬬暀浜嬩欢鍦ㄦ柊椤甸潰鍐嶆瑙﹀彂鎸夐挳銆? */
static void drain_touch_events(void) {
    struct input_event ev;

    while (read(g_touch_fd, &ev, sizeof(ev)) == sizeof(ev)) {
    }
}

/*
 * 缁樺埗褰曞叆濮撳悕鐢ㄧ殑 26 閿嫳鏂囬敭鐩樸€? * 褰曞叆鎴愬姛鍚庡脊鍑鸿椤甸潰锛岀敤浜庤緭鍏ヤ汉鍛樺鍚嶅墠缂€锛屼緥濡?fqr銆? */
static void draw_name_keyboard(const char *name) {
    static const char rows[3][11] = {
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm"
    };
    const int row_len[3] = {10, 9, 7};
    const int row_x[3] = {35, 80, 170};
    const int row_y[3] = {210, 305, 400};
    const int key_w = 82;
    const int key_h = 70;
    const int gap = 8;

    memset(g_drm.vaddr, 0, SCREEN_W * SCREEN_H * 4);
    fill_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 245, 245, 245);
    draw_text(45, 35, "input face name", 4, 20, 20, 20);
    fill_rect(45, 95, 820, 155, 255, 255, 255);
    draw_rect_border(45, 95, 820, 155, 3, 80, 80, 80);
    draw_text(65, 112, name, 4, 0, 0, 0);

    for (int row = 0; row < 3; row++) {
        for (int i = 0; i < row_len[row]; i++) {
            int x1 = row_x[row] + i * (key_w + gap);
            int y1 = row_y[row];
            int x2 = x1 + key_w;
            int y2 = y1 + key_h;
            fill_rect(x1, y1, x2, y2, 255, 255, 255);
            draw_rect_border(x1, y1, x2, y2, 3, 120, 120, 120);
            draw_char(x1 + 31, y1 + 18, rows[row][i], 5, 0, 0, 0);
        }
    }

    fill_rect(820, 400, 990, 470, 230, 230, 230);
    draw_rect_border(820, 400, 990, 470, 3, 80, 80, 80);
    draw_text(842, 422, "del", 4, 0, 0, 0);

    fill_rect(820, 500, 990, 575, 90, 180, 90);
    draw_rect_border(820, 500, 990, 575, 3, 20, 100, 20);
    draw_text(858, 522, "ok", 5, 255, 255, 255);

    DRMshowUp(g_drm_fd, &g_drm);
}

/* 鏍规嵁瑙︽懜鍧愭爣鍒ゆ柇閿洏涓婅鎸変笅鐨勫瓧绗︺€佸垹闄ら敭鎴栫‘璁ら敭銆?*/
static int keyboard_key_at(int x, int y, char *out_ch) {
    static const char rows[3][11] = {
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm"
    };
    const int row_len[3] = {10, 9, 7};
    const int row_x[3] = {35, 80, 170};
    const int row_y[3] = {210, 305, 400};
    const int key_w = 82;
    const int key_h = 70;
    const int gap = 8;

    if (in_rect(x, y, 820, 400, 990, 470)) {
        *out_ch = '\b';
        return 1;
    }
    if (in_rect(x, y, 820, 500, 990, 575)) {
        *out_ch = '\n';
        return 1;
    }

    for (int row = 0; row < 3; row++) {
        for (int i = 0; i < row_len[row]; i++) {
            int x1 = row_x[row] + i * (key_w + gap);
            int y1 = row_y[row];
            if (in_rect(x, y, x1, y1, x1 + key_w, y1 + key_h)) {
                *out_ch = rows[row][i];
                return 1;
            }
        }
    }

    return 0;
}

/*
 * 闃诲杈撳叆浜鸿劯濮撳悕銆? * 鍙厑璁搁€氳繃灞忓箷閿洏杈撳叆灏忓啓鑻辨枃锛屾渶缁堢敤浜庣敓鎴愶細
 *     ./face/濮撳悕_face/濮撳悕_001.bmp
 */
static int input_face_name(char *name, size_t name_size) {
    size_t len = 0;
    int x = 0;
    int y = 0;

    if (name_size == 0) {
        return -1;
    }
    name[0] = '\0';
    draw_name_keyboard(name);

    while (g_running) {
        if (wait_touch(&x, &y) <= 0) {
            return -1;
        }

        char ch = 0;
        if (!keyboard_key_at(x, y, &ch)) {
            continue;
        }

        if (ch == '\n') {
            if (len > 0) {
                return 0;
            }
            continue;
        }
        if (ch == '\b') {
            if (len > 0) {
                len--;
                name[len] = '\0';
            }
            draw_name_keyboard(name);
            continue;
        }
        if (len + 1 < name_size) {
            name[len++] = ch;
            name[len] = '\0';
            draw_name_keyboard(name);
        }
    }

    return -1;
}

/*
 * 闈為樆濉炶Е鎽歌疆璇€? * 褰曞叆椤?鎵撳崱椤甸渶瑕佷竴杈瑰埛鏂版憚鍍忓ご鐢婚潰锛屼竴杈规娴嬫寜閽偣鍑伙紝鍥犳涓嶈兘鐢?wait_touch() 闃诲銆? */
static int poll_touch(int *out_x, int *out_y) {
    static int last_x = -1;
    static int last_y = -1;
    static int syn_debounce = 0;
    static int locked = 0;
    struct input_event ev;

    while (1) {
        ssize_t n = read(g_touch_fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        }

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                last_x = ev.value;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                last_y = ev.value;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0) {
            if (last_x >= 0 && last_y >= 0) {
                locked = 0;
                *out_x = last_x;
                *out_y = last_y;
                printf("touch x=%d y=%d\n", last_x, last_y);
                usleep(120000);
                drain_touch_events();
                return 1;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT && last_x >= 0 && last_y >= 0) {
            if (!locked && syn_debounce <= 0) {
                *out_x = last_x;
                *out_y = last_y;
                printf("touch x=%d y=%d\n", last_x, last_y);
                syn_debounce = 12;
                locked = 1;
                return 1;
            }
            syn_debounce--;
            if (syn_debounce <= 0) {
                locked = 0;
            }
        }
    }
}

/* 灏嗗睆骞曟寚瀹氬尯鍩熸竻涓洪粦鑹层€?*/
static void clear_screen_rect(int start_x, int start_y, int width, int height) {
    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL) {
        return;
    }

    for (int y = 0; y < height; y++) {
        int screen_y = start_y + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }

        for (int x = 0; x < width; x++) {
            int screen_x = start_x + x;
            if (screen_x < 0 || screen_x >= SCREEN_W) {
                continue;
            }

            int pos = 4 * (SCREEN_W * screen_y + screen_x);
            screen[pos + 0] = 0;
            screen[pos + 1] = 0;
            screen[pos + 2] = 0;
            screen[pos + 3] = 0;
        }
    }
}

/*
 * 缂╂斁缁樺埗 BMP 鍒版寚瀹氬尯鍩熴€? * 褰撳墠瀹炴椂棰勮宸叉敼涓?RGB 鐩存樉锛岃鍑芥暟淇濈暀浣滀负澶囩敤宸ュ叿銆? */
static int bmp_draw_scaled(const char *bmp_path, int dst_x, int dst_y, int dst_w, int dst_h) {
    int bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd == -1) {
        perror("open bmp file error");
        return -1;
    }

    unsigned char header[54];
    if (read_all(bmp_fd, header, sizeof(header)) == -1) {
        perror("read bmp header error");
        close(bmp_fd);
        return -1;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "%s is not a BMP file\n", bmp_path);
        close(bmp_fd);
        return -1;
    }

    int data_offset = *(int *)&header[10];
    int w = *(int *)&header[18];
    int h = *(int *)&header[22];
    short bits = *(short *)&header[28];

    if (w <= 0 || h <= 0 || dst_w <= 0 || dst_h <= 0 || (bits != 24 && bits != 32)) {
        fprintf(stderr, "unsupported bmp: width=%d height=%d bits=%d\n", w, h, bits);
        close(bmp_fd);
        return -1;
    }

    int bytes_per_pixel = bits / 8;
    int row_size = ((w * bytes_per_pixel + 3) / 4) * 4;
    size_t bmp_size = (size_t)row_size * h;
    unsigned char *bmp_buf = malloc(bmp_size);
    if (bmp_buf == NULL) {
        perror("malloc bmp buffer error");
        close(bmp_fd);
        return -1;
    }

    if (lseek(bmp_fd, data_offset, SEEK_SET) == -1 ||
        read_all(bmp_fd, bmp_buf, bmp_size) == -1) {
        perror("read bmp data error");
        free(bmp_buf);
        close(bmp_fd);
        return -1;
    }
    close(bmp_fd);

    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL) {
        fprintf(stderr, "drm.vaddr is NULL\n");
        free(bmp_buf);
        return -1;
    }

    for (int y = 0; y < dst_h; y++) {
        int screen_y = dst_y + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }

        int src_y = y * h / dst_h;
        for (int x = 0; x < dst_w; x++) {
            int screen_x = dst_x + x;
            if (screen_x < 0 || screen_x >= SCREEN_W) {
                continue;
            }

            int src_x = x * w / dst_w;
            int pos = 4 * (SCREEN_W * screen_y + screen_x);
            int bmp_pos = row_size * (h - 1 - src_y) + bytes_per_pixel * src_x;

            screen[pos + 0] = bmp_buf[bmp_pos + 0];
            screen[pos + 1] = bmp_buf[bmp_pos + 1];
            screen[pos + 2] = bmp_buf[bmp_pos + 2];
            screen[pos + 3] = 0;
        }
    }

    free(bmp_buf);
    return 0;
}

/*
 * 涓嶇缉鏀惧湴缁樺埗 BMP 鍒版寚瀹氬尯鍩燂紝瓒呭嚭鍖哄煙瑁佸壀锛岀┖鐧藉尯鍩熶繚鎸侀粦鑹层€? * 鏃╂湡棰勮鏇剧敤璇ュ嚱鏁版樉绀?BMP 鏂囦欢銆? */
static int bmp_draw_no_scale_region(const char *bmp_path,
                                    int dst_x,
                                    int dst_y,
                                    int region_w,
                                    int region_h) {
    int bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd == -1) {
        perror("open bmp file error");
        return -1;
    }

    unsigned char header[54];
    if (read_all(bmp_fd, header, sizeof(header)) == -1) {
        perror("read bmp header error");
        close(bmp_fd);
        return -1;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "%s is not a BMP file\n", bmp_path);
        close(bmp_fd);
        return -1;
    }

    int data_offset = *(int *)&header[10];
    int w = *(int *)&header[18];
    int h = *(int *)&header[22];
    short bits = *(short *)&header[28];

    if (w <= 0 || h <= 0 || region_w <= 0 || region_h <= 0 || (bits != 24 && bits != 32)) {
        fprintf(stderr, "unsupported bmp: width=%d height=%d bits=%d\n", w, h, bits);
        close(bmp_fd);
        return -1;
    }

    int bytes_per_pixel = bits / 8;
    int row_size = ((w * bytes_per_pixel + 3) / 4) * 4;
    size_t bmp_size = (size_t)row_size * h;
    unsigned char *bmp_buf = malloc(bmp_size);
    if (bmp_buf == NULL) {
        perror("malloc bmp buffer error");
        close(bmp_fd);
        return -1;
    }

    if (lseek(bmp_fd, data_offset, SEEK_SET) == -1 ||
        read_all(bmp_fd, bmp_buf, bmp_size) == -1) {
        perror("read bmp data error");
        free(bmp_buf);
        close(bmp_fd);
        return -1;
    }
    close(bmp_fd);

    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL) {
        fprintf(stderr, "drm.vaddr is NULL\n");
        free(bmp_buf);
        return -1;
    }

    clear_screen_rect(dst_x, dst_y, region_w, region_h);

    int draw_w = w < region_w ? w : region_w;
    int draw_h = h < region_h ? h : region_h;
    for (int y = 0; y < draw_h; y++) {
        int screen_y = dst_y + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }

        for (int x = 0; x < draw_w; x++) {
            int screen_x = dst_x + x;
            if (screen_x < 0 || screen_x >= SCREEN_W) {
                continue;
            }

            int pos = 4 * (SCREEN_W * screen_y + screen_x);
            int bmp_pos = row_size * (h - 1 - y) + bytes_per_pixel * x;

            screen[pos + 0] = bmp_buf[bmp_pos + 0];
            screen[pos + 1] = bmp_buf[bmp_pos + 1];
            screen[pos + 2] = bmp_buf[bmp_pos + 2];
            screen[pos + 3] = 0;
        }
    }

    free(bmp_buf);
    return 0;
}

/*
 * 鏄剧ず鏁村紶 BMP 椤甸潰鍥撅紝骞跺埛鏂板埌灞忓箷銆? * 棣栭〉銆佸綍鍏ラ〉銆佹墦鍗￠〉銆佹垚鍔?澶辫触椤甸兘閫氳繃璇ュ嚱鏁版樉绀恒€? */
static int bmp_show(const char *bmp_path, int start_x, int start_y) {
    int bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd == -1) {
        perror("open bmp file error");
        return -1;
    }

    unsigned char header[54];
    if (read_all(bmp_fd, header, sizeof(header)) == -1) {
        perror("read bmp header error");
        close(bmp_fd);
        return -1;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "%s is not a BMP file\n", bmp_path);
        close(bmp_fd);
        return -1;
    }

    int data_offset = *(int *)&header[10];
    int w = *(int *)&header[18];
    int h = *(int *)&header[22];
    short bits = *(short *)&header[28];

    if (w <= 0 || h <= 0 || (bits != 24 && bits != 32)) {
        fprintf(stderr, "unsupported bmp: width=%d height=%d bits=%d\n", w, h, bits);
        close(bmp_fd);
        return -1;
    }

    int bytes_per_pixel = bits / 8;
    int row_size = ((w * bytes_per_pixel + 3) / 4) * 4;
    size_t bmp_size = (size_t)row_size * h;
    unsigned char *bmp_buf = malloc(bmp_size);
    if (bmp_buf == NULL) {
        perror("malloc bmp buffer error");
        close(bmp_fd);
        return -1;
    }

    if (lseek(bmp_fd, data_offset, SEEK_SET) == -1 ||
        read_all(bmp_fd, bmp_buf, bmp_size) == -1) {
        perror("read bmp data error");
        free(bmp_buf);
        close(bmp_fd);
        return -1;
    }
    close(bmp_fd);

    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL) {
        fprintf(stderr, "drm.vaddr is NULL\n");
        free(bmp_buf);
        return -1;
    }

    memset(screen, 0, SCREEN_W * SCREEN_H * 4);

    for (int y = 0; y < h; y++) {
        int screen_y = start_y + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }

        for (int x = 0; x < w; x++) {
            int screen_x = start_x + x;
            if (screen_x < 0 || screen_x >= SCREEN_W) {
                continue;
            }

            int pos = 4 * (SCREEN_W * screen_y + screen_x);
            int bmp_pos = row_size * (h - 1 - y) + bytes_per_pixel * x;

            screen[pos + 0] = bmp_buf[bmp_pos + 0];
            screen[pos + 1] = bmp_buf[bmp_pos + 1];
            screen[pos + 2] = bmp_buf[bmp_pos + 2];
            screen[pos + 3] = 0;
        }
    }

    DRMshowUp(g_drm_fd, &g_drm);
    free(bmp_buf);
    return 0;
}

/*
 * 鍒濆鍖?RockX 妯″瀷銆? * 绋嬪簭闇€瑕佸悓鏃朵娇鐢細
 * - 浜鸿劯妫€娴? * - 浜旂偣鍏抽敭鐐瑰榻? * - 浜鸿劯鐗瑰緛璇嗗埆
 */
static int face_init(void) {
    rockx_ret_t ret;

    ret = rockx_create(&g_face_det_handle, ROCKX_MODULE_FACE_DETECTION, NULL, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init detection module error\n");
        return -1;
    }
    g_face_det_ready = 1;

    ret = rockx_create(&g_face_5landmarks_handle, ROCKX_MODULE_FACE_LANDMARK_5, NULL, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init landmarks module error\n");
        return -1;
    }
    g_face_5landmarks_ready = 1;

    ret = rockx_create(&g_face_recognize_handle, ROCKX_MODULE_FACE_RECOGNIZE, NULL, 0);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("init recognize module error\n");
        return -1;
    }
    g_face_recognize_ready = 1;

    return 0;
}

/* 浠庢娴嬪埌鐨勪汉鑴镐腑閫夋嫨闈㈢Н鏈€澶х殑浜鸿劯锛屼綔涓哄綍鍏?鎵撳崱鐨勭洰鏍囦汉鑴搞€?*/
static rockx_object_t *get_max_face(rockx_object_array_t *face_array) {
    rockx_object_t *max_face = NULL;

    for (int i = 0; i < face_array->count; i++) {
        rockx_object_t *cur_face = &face_array->object[i];
        if (max_face == NULL) {
            max_face = cur_face;
            continue;
        }

        int cur_area = (cur_face->box.right - cur_face->box.left) *
                       (cur_face->box.bottom - cur_face->box.top);
        int max_area = (max_face->box.right - max_face->box.left) *
                       (max_face->box.bottom - max_face->box.top);
        if (cur_area > max_area) {
            max_face = cur_face;
        }
    }

    return max_face;
}

/*
 * 浠?BMP 鏂囦欢涓彁鍙栦汉鑴哥壒寰併€? * 涓昏鐢ㄤ簬鍖归厤浜鸿劯搴撲腑宸茬粡淇濆瓨鐨勫浘鐗囷細./face/*_face/*.bmp銆? */
static int extract_face_feature(const char *image_path, rockx_face_feature_t *out_feature) {
    rockx_image_t in_face;
    rockx_image_t small_face;
    rockx_object_array_t face_array;

    memset(&in_face, 0, sizeof(in_face));
    memset(&small_face, 0, sizeof(small_face));
    memset(&face_array, 0, sizeof(face_array));
    memset(out_feature, 0, sizeof(*out_feature));

    if (rockx_image_read(image_path, &in_face, 1) != ROCKX_RET_SUCCESS) {
        printf("read image failed: %s\n", image_path);
        return -1;
    }

    rockx_ret_t ret = rockx_face_detect(g_face_det_handle, &in_face, &face_array, NULL);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("detect face failed: %s\n", image_path);
        rockx_image_release(&in_face);
        return -1;
    }

    rockx_image_t *draw_img = rockx_image_clone(&in_face);
    if (draw_img != NULL) {
        for (int i = 0; i < face_array.count; i++) {
            rockx_point_t pt1 = {face_array.object[i].box.left, face_array.object[i].box.top};
            rockx_point_t pt2 = {face_array.object[i].box.right, face_array.object[i].box.bottom};
            rockx_color_t color = {255, 0, 0};
            rockx_image_draw_rect(draw_img, pt1, pt2, color, 3);
        }
        rockx_image_write(KEEP_FACE_BMP, draw_img);
        rockx_image_release(draw_img);
    }

    rockx_object_t *max_face = get_max_face(&face_array);
    if (max_face == NULL) {
        printf("no face found: %s\n", image_path);
        rockx_image_release(&in_face);
        return -1;
    }

    printf("face count=%d max box left=%d top=%d right=%d bottom=%d score=%f\n",
           face_array.count,
           max_face->box.left,
           max_face->box.top,
           max_face->box.right,
           max_face->box.bottom,
           max_face->score);

    ret = rockx_face_align(g_face_5landmarks_handle, &in_face, &max_face->box, NULL, &small_face);
    if (ret != ROCKX_RET_SUCCESS) {
        rockx_rect_t safe_box = max_face->box;
        int bw = safe_box.right - safe_box.left;
        int bh = safe_box.bottom - safe_box.top;
        int margin_x = bw / 20;
        int margin_y = bh / 20;

        safe_box.left += margin_x;
        safe_box.right -= margin_x;
        safe_box.top += margin_y;
        safe_box.bottom -= margin_y;

        printf("face align failed ret=%d, retry with safe box left=%d top=%d right=%d bottom=%d\n",
               ret, safe_box.left, safe_box.top, safe_box.right, safe_box.bottom);

        memset(&small_face, 0, sizeof(small_face));
        ret = rockx_face_align(g_face_5landmarks_handle, &in_face, &safe_box, NULL, &small_face);
        if (ret != ROCKX_RET_SUCCESS) {
            printf("face align retry failed ret=%d: %s\n", ret, image_path);
            rockx_image_release(&in_face);
            return -1;
        }
    }

    ret = rockx_face_recognize(g_face_recognize_handle, &small_face, out_feature);
    rockx_image_release(&in_face);
    rockx_image_release(&small_face);

    if (ret != ROCKX_RET_SUCCESS) {
        printf("face recognize failed: %s\n", image_path);
        return -1;
    }

    return 0;
}

/*
 * 灏嗘憚鍍忓ご RGB 鍐呭瓨甯х洿鎺ョ粯鍒跺埌 DRM 鏄惧瓨銆? * 杩欐槸褰撳墠瀹炴椂棰勮鐨勬牳蹇冧紭鍖栫偣锛氫笉鍐嶆瘡甯т繚瀛?BMP锛屼篃涓嶅啀姣忓抚璇诲彇 BMP銆? */
static void rgb_draw_no_scale_region(unsigned char *rgb,
                                     int dst_x,
                                     int dst_y,
                                     int region_w,
                                     int region_h) {
    unsigned char *screen = g_drm.vaddr;
    if (screen == NULL || rgb == NULL) {
        return;
    }

    clear_screen_rect(dst_x, dst_y, region_w, region_h);

    int draw_w = 640 < region_w ? 640 : region_w;
    int draw_h = 480 < region_h ? 480 : region_h;
    for (int y = 0; y < draw_h; y++) {
        int screen_y = dst_y + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }

        for (int x = 0; x < draw_w; x++) {
            int screen_x = dst_x + x;
            if (screen_x < 0 || screen_x >= SCREEN_W) {
                continue;
            }

            int pos = 4 * (SCREEN_W * screen_y + screen_x);
            int rgb_pos = 3 * (640 * y + x);
            screen[pos + 0] = rgb[rgb_pos + 2];
            screen[pos + 1] = rgb[rgb_pos + 1];
            screen[pos + 2] = rgb[rgb_pos + 0];
            screen[pos + 3] = 0;
        }
    }
}

/* 灏?640x480 RGB 鍐呭瓨灏佽鎴?RockX 鍙互璇嗗埆鐨?rockx_image_t銆?*/
static void make_rgb_image(unsigned char *rgb, rockx_image_t *image) {
    memset(image, 0, sizeof(*image));
    image->data = rgb;
    image->size = 640 * 480 * 3;
    image->is_prealloc_buf = 1;
    image->pixel_format = ROCKX_PIXEL_FORMAT_RGB888;
    image->width = 640;
    image->height = 480;
}

/*
 * 浠庡綋鍓嶆憚鍍忓ご RGB 鍐呭瓨甯т腑鐩存帴鎻愬彇浜鸿劯鐗瑰緛銆? * 褰曞叆鍜屾墦鍗＄偣鍑绘椂浣跨敤璇ュ嚱鏁帮紝閬垮厤鍐嶄粠 BMP 鏂囦欢璇诲洖褰撳墠甯с€? */
static int extract_face_feature_from_rgb(unsigned char *rgb, rockx_face_feature_t *out_feature) {
    rockx_image_t in_face;
    rockx_image_t small_face;
    rockx_object_array_t face_array;

    make_rgb_image(rgb, &in_face);
    memset(&small_face, 0, sizeof(small_face));
    memset(&face_array, 0, sizeof(face_array));
    memset(out_feature, 0, sizeof(*out_feature));

    rockx_ret_t ret = rockx_face_detect(g_face_det_handle, &in_face, &face_array, NULL);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("detect face failed from rgb\n");
        return -1;
    }

    rockx_object_t *max_face = get_max_face(&face_array);
    if (max_face == NULL) {
        printf("no face found from rgb\n");
        return -1;
    }

    ret = rockx_face_align(g_face_5landmarks_handle, &in_face, &max_face->box, NULL, &small_face);
    if (ret != ROCKX_RET_SUCCESS) {
        rockx_rect_t safe_box = max_face->box;
        int bw = safe_box.right - safe_box.left;
        int bh = safe_box.bottom - safe_box.top;
        int margin_x = bw / 20;
        int margin_y = bh / 20;

        safe_box.left += margin_x;
        safe_box.right -= margin_x;
        safe_box.top += margin_y;
        safe_box.bottom -= margin_y;

        memset(&small_face, 0, sizeof(small_face));
        ret = rockx_face_align(g_face_5landmarks_handle, &in_face, &safe_box, NULL, &small_face);
        if (ret != ROCKX_RET_SUCCESS) {
            printf("face align retry failed from rgb ret=%d\n", ret);
            return -1;
        }
    }

    ret = rockx_face_recognize(g_face_recognize_handle, &small_face, out_feature);
    rockx_image_release(&small_face);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("face recognize failed from rgb\n");
        return -1;
    }

    return 0;
}

/*
 * 鏃х増 BMP 鐢绘鍑芥暟銆? * 褰撳墠瀹炴椂棰勮宸茬粡鏀逛负鍐呭瓨鐩存樉骞剁洿鎺ョ敾妗嗭紝璇ュ嚱鏁颁繚鐣欏吋瀹瑰鐢ㄣ€? */
static int make_face_box_image(const char *src_path, const char *dst_path) {
    rockx_image_t image;
    rockx_object_array_t face_array;

    memset(&image, 0, sizeof(image));
    memset(&face_array, 0, sizeof(face_array));

    if (rockx_image_read(src_path, &image, 1) != ROCKX_RET_SUCCESS) {
        printf("read preview image failed: %s\n", src_path);
        return -1;
    }

    rockx_ret_t ret = rockx_face_detect(g_face_det_handle, &image, &face_array, NULL);
    if (ret != ROCKX_RET_SUCCESS) {
        printf("preview face detect failed: %s\n", src_path);
        rockx_image_release(&image);
        return -1;
    }

    rockx_image_t *draw_img = rockx_image_clone(&image);
    if (draw_img == NULL) {
        rockx_image_release(&image);
        return -1;
    }

    for (int i = 0; i < face_array.count; i++) {
        rockx_point_t pt1 = {face_array.object[i].box.left, face_array.object[i].box.top};
        rockx_point_t pt2 = {face_array.object[i].box.right, face_array.object[i].box.bottom};
        rockx_color_t color = {255, 0, 0};
        rockx_image_draw_rect(draw_img, pt1, pt2, color, 3);
    }

    ret = rockx_image_write(dst_path, draw_img);
    rockx_image_release(draw_img);
    rockx_image_release(&image);

    if (ret != ROCKX_RET_SUCCESS) {
        printf("write preview image failed: %s\n", dst_path);
        return -1;
    }

    return 0;
}

/*
 * 鍒锋柊瀹炴椂棰勮鐢婚潰銆? *
 * 褰撳墠娴佺▼锛? * 1. get_rgb_frame() 浠庢憚鍍忓ご閲囬泦 RGB 鍐呭瓨甯? * 2. rgb_draw_no_scale_region() 鐩存帴鏄剧ず鍒?DRM
 * 3. rockx_face_detect() 鐩存帴妫€娴嬪唴瀛樺抚
 * 4. draw_rect_border() 鍦ㄥ睆骞曚笂缁樺埗浜鸿劯妗? */
static int update_face_preview(const char *raw_path, const char *boxed_path) {
    (void)raw_path;
    (void)boxed_path;

    if (get_rgb_frame(g_video_fd, g_preview_rgb, sizeof(g_preview_rgb)) == -1) {
        printf("camera preview capture failed\n");
        return -1;
    }

    rgb_draw_no_scale_region(g_preview_rgb, PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H);

    rockx_image_t image;
    rockx_object_array_t face_array;
    make_rgb_image(g_preview_rgb, &image);
    memset(&face_array, 0, sizeof(face_array));
    if (rockx_face_detect(g_face_det_handle, &image, &face_array, NULL) == ROCKX_RET_SUCCESS) {
        for (int i = 0; i < face_array.count; i++) {
            int x1 = PREVIEW_X + face_array.object[i].box.left;
            int y1 = PREVIEW_Y + face_array.object[i].box.top;
            int x2 = PREVIEW_X + face_array.object[i].box.right;
            int y2 = PREVIEW_Y + face_array.object[i].box.bottom;

            if (x1 < PREVIEW_X) x1 = PREVIEW_X;
            if (y1 < PREVIEW_Y) y1 = PREVIEW_Y;
            if (x2 >= PREVIEW_X + PREVIEW_W) x2 = PREVIEW_X + PREVIEW_W - 1;
            if (y2 >= PREVIEW_Y + PREVIEW_H) y2 = PREVIEW_Y + PREVIEW_H - 1;

            draw_rect_border(x1, y1, x2, y2, 3, 255, 0, 0);
        }
    }

    DRMshowUp(g_drm_fd, &g_drm);
    return 0;
}

/*
 * 鏃х増鈥滄媿鐓у苟鎻愬彇鐗瑰緛鈥濇帴鍙ｃ€? * 褰撳墠褰曞叆/鎵撳崱宸叉敼涓?RGB 鍐呭瓨鎻愬彇锛岃鍑芥暟淇濈暀鍏煎澶囩敤銆? */
static int capture_feature(const char *bmp_path, rockx_face_feature_t *feature) {
    if (get_bmp(g_video_fd, bmp_path) == -1) {
        printf("camera capture failed\n");
        return -1;
    }

    return extract_face_feature(bmp_path, feature);
}

/* 鏄剧ず鎴愬姛/澶辫触椤甸潰 1 绉掞紝鐒跺悗鎭㈠鍒版寚瀹氶〉闈€?*/
static void show_result_then_restore(const char *result_bmp, const char *page_bmp) {
    bmp_show(result_bmp, 0, 0);
    sleep(1);
    bmp_show(page_bmp, 0, 0);
    draw_back_button();
    DRMshowUp(g_drm_fd, &g_drm);
}

/* 鎵撳崱鎴愬姛鏃舵樉绀?3_success.bmp锛屽苟鍦ㄩ《閮ㄥ眳涓樉绀?Hi,濮撳悕銆?*/
static void show_check_success_with_name(const char *face_name) {
    char text[80];
    int n = snprintf(text, sizeof(text), "Hi,%s", face_name);
    if (n < 0 || (size_t)n >= sizeof(text)) {
        snprintf(text, sizeof(text), "Hi");
    }

    bmp_show(CHECK_SUCCESS_BMP, 0, 0);
    int scale = 6;
    int text_w = (int)strlen(text) * 6 * scale;
    int x = (SCREEN_W - text_w) / 2;
    if (x < 0) {
        x = 0;
    }
    fill_rect(0, 10, SCREEN_W - 1, 75, 255, 255, 255);
    draw_text(x, 22, text, scale, 255, 0, 0);
    DRMshowUp(g_drm_fd, &g_drm);
    sleep(1);
    bmp_show("./3.bmp", 0, 0);
    draw_back_button();
    DRMshowUp(g_drm_fd, &g_drm);
}

/* 鏅€氭枃浠跺鍒讹紝鐢ㄤ簬淇濆瓨褰曞叆浜鸿劯鍜屾墦鍗¤褰曘€?*/
static int copy_file(const char *src_path, const char *dst_path) {
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd == -1) {
        perror("open source face image error");
        return -1;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("open destination face image error");
        close(src_fd);
        return -1;
    }

    unsigned char buf[4096];
    while (1) {
        ssize_t n = read(src_fd, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        if (n < 0) {
            perror("read source face image error");
            close(src_fd);
            close(dst_fd);
            return -1;
        }

        ssize_t written = 0;
        while (written < n) {
            ssize_t m = write(dst_fd, buf + written, (size_t)(n - written));
            if (m <= 0) {
                perror("write destination face image error");
                close(src_fd);
                close(dst_fd);
                return -1;
            }
            written += m;
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}

/* 鍒ゆ柇瀛楃涓叉槸鍚︿互鎸囧畾鍚庣紑缁撳熬锛屼緥濡?.bmp銆乢face銆?*/
static int has_suffix(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    return name_len >= suffix_len && strcmp(name + name_len - suffix_len, suffix) == 0;
}

/* 鍒ゆ柇璺緞鏄惁涓烘櫘閫氭枃浠躲€?*/
static int is_regular_file_path(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* 鍒ゆ柇璺緞鏄惁涓虹洰褰曘€?*/
static int is_directory_path(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* 浜鸿劯搴撶洰褰曞懡鍚嶈鍒欙細濮撳悕_face銆?*/
static int is_named_face_dir(const char *name) {
    return strlen(name) > strlen("_face") && has_suffix(name, "_face");
}

/*
 * 鑾峰彇鏌愪釜浜哄憳鐩綍涓殑涓嬩竴涓浘鐗囩紪鍙枫€? * 渚嬪 fqr_face 宸叉湁 fqr_001.bmp锛屽垯杩斿洖 2銆? */
static int get_next_named_face_index(const char *name) {
    char dir_path[256];
    int n = snprintf(dir_path, sizeof(dir_path), FACE_DIR "/%s_face", name);
    if (n < 0 || (size_t)n >= sizeof(dir_path)) {
        fprintf(stderr, "face dir path too long\n");
        return -1;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return 1;
        }
        perror("open named face dir error");
        return -1;
    }

    int max_index = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char prefix[64];
        n = snprintf(prefix, sizeof(prefix), "%s_", name);
        if (n < 0 || (size_t)n >= sizeof(prefix)) {
            closedir(dir);
            return -1;
        }

        size_t prefix_len = strlen(prefix);
        size_t entry_len = strlen(entry->d_name);
        if (entry_len <= prefix_len + 4 ||
            strncmp(entry->d_name, prefix, prefix_len) != 0 ||
            strcmp(entry->d_name + entry_len - 4, ".bmp") != 0) {
            continue;
        }

        int index = atoi(entry->d_name + prefix_len);
        if (index > max_index) {
            max_index = index;
        }
    }

    closedir(dir);
    return max_index + 1;
}

/*
 * 淇濆瓨褰曞叆鎴愬姛鐨勪汉鑴稿浘鐗囥€? * 杈撳叆 name=fqr 鏃讹紝淇濆瓨鍒帮細
 *     ./face/fqr_face/fqr_001.bmp
 */
static int save_enrolled_face_image(const char *src_path, const char *name) {
    char dir_path[256];
    int n = snprintf(dir_path, sizeof(dir_path), FACE_DIR "/%s_face", name);
    if (n < 0 || (size_t)n >= sizeof(dir_path)) {
        fprintf(stderr, "named face dir path too long\n");
        return -1;
    }

    if (mkdir(dir_path, 0777) == -1 && errno != EEXIST) {
        perror("mkdir named face dir error");
        return -1;
    }

    int index = get_next_named_face_index(name);
    if (index <= 0) {
        return -1;
    }

    char dst_path[256];
    n = snprintf(dst_path, sizeof(dst_path), "%s/%s_%03d.bmp", dir_path, name, index);
    if (n < 0 || (size_t)n >= sizeof(dst_path)) {
        fprintf(stderr, "face image path too long\n");
        return -1;
    }

    if (copy_file(src_path, dst_path) == -1) {
        return -1;
    }

    printf("saved enrolled face: %s\n", dst_path);
    return 0;
}

/*
 * 淇濆瓨鎵撳崱鎴愬姛璁板綍銆? * 鏂囦欢鍚嶆牸寮忥細
 *     ./record/濮撳悕_骞存湀鏃ュ皬鏃跺垎閽?bmp
 */
static int save_check_record_image(const char *src_path, const char *name) {
    if (ensure_record_dir() == -1) {
        return -1;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    struct tm *tm_ptr = localtime(&now);
    if (tm_ptr == NULL) {
        perror("localtime error");
        return -1;
    }
    tm_now = *tm_ptr;

    char time_text[32];
    if (strftime(time_text, sizeof(time_text), "%Y%m%d%H%M", &tm_now) == 0) {
        fprintf(stderr, "format record time failed\n");
        return -1;
    }

    char dst_path[256];
    int n = snprintf(dst_path, sizeof(dst_path), RECORD_DIR "/%s_%s.bmp", name, time_text);
    if (n < 0 || (size_t)n >= sizeof(dst_path)) {
        fprintf(stderr, "record image path too long\n");
        return -1;
    }

    for (int i = 1; is_regular_file_path(dst_path); i++) {
        n = snprintf(dst_path, sizeof(dst_path), RECORD_DIR "/%s_%s_%02d.bmp", name, time_text, i);
        if (n < 0 || (size_t)n >= sizeof(dst_path)) {
            fprintf(stderr, "record image path too long\n");
            return -1;
        }
    }

    if (copy_file(src_path, dst_path) == -1) {
        return -1;
    }

    printf("saved check record: %s\n", dst_path);
    return 0;
}

typedef struct {
    char name[128];
    char path[256];
    int count;
} admin_item_t;

/* 缁熻鐩綍涓?BMP 鏂囦欢鏁伴噺銆?*/
static int count_bmp_files_in_dir(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return 0;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!has_suffix(entry->d_name, ".bmp")) {
            continue;
        }

        char path[256];
        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n >= 0 && (size_t)n < sizeof(path) && is_regular_file_path(path)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

/* 缁熻浜鸿劯搴撲腑鏈夊灏戜釜 濮撳悕_face 鐩綍銆?*/
static int count_face_dirs(void) {
    DIR *dir = opendir(FACE_DIR);
    if (dir == NULL) {
        return 0;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!is_named_face_dir(entry->d_name)) {
            continue;
        }

        char path[256];
        int n = snprintf(path, sizeof(path), FACE_DIR "/%s", entry->d_name);
        if (n >= 0 && (size_t)n < sizeof(path) && is_directory_path(path)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

/* 缁熻 record 鐩綍涓嬫湁澶氬皯鏉℃墦鍗¤褰曘€?*/
static int count_record_files(void) {
    return count_bmp_files_in_dir(RECORD_DIR);
}

/*
 * 鏀堕泦绠＄悊鍛樼晫闈㈠綋鍓嶉〉瑕佹樉绀虹殑鏁版嵁銆? * tab 涓轰汉鑴稿簱鏃舵敹闆?*_face 鐩綍锛泃ab 涓鸿褰曟椂鏀堕泦 record/*.bmp銆? */
static int collect_admin_items(int tab, int page, admin_item_t *items, int max_items, int *total_items) {
    const char *base_dir = tab == ADMIN_TAB_FACES ? FACE_DIR : RECORD_DIR;
    DIR *dir = opendir(base_dir);
    if (dir == NULL) {
        if (total_items != NULL) {
            *total_items = 0;
        }
        return 0;
    }

    int total = 0;
    int out_count = 0;
    int start = page * max_items;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char path[256];
        int n = snprintf(path, sizeof(path), "%s/%s", base_dir, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            continue;
        }

        int valid = 0;
        int file_count = 0;
        if (tab == ADMIN_TAB_FACES) {
            valid = is_named_face_dir(entry->d_name) && is_directory_path(path);
            if (valid) {
                file_count = count_bmp_files_in_dir(path);
            }
        } else {
            valid = has_suffix(entry->d_name, ".bmp") && is_regular_file_path(path);
        }

        if (!valid) {
            continue;
        }

        if (total >= start && out_count < max_items) {
            snprintf(items[out_count].name, sizeof(items[out_count].name), "%s", entry->d_name);
            snprintf(items[out_count].path, sizeof(items[out_count].path), "%s", path);
            items[out_count].count = file_count;
            out_count++;
        }
        total++;
    }

    closedir(dir);
    if (total_items != NULL) {
        *total_items = total;
    }
    return out_count;
}

/* 閫掑綊鍒犻櫎鐩綍锛岀鐞嗗憳鍒犻櫎鏌愪釜浜鸿劯搴撴椂浣跨敤銆?*/
static int delete_dir_recursive(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("open delete dir error");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[256];
        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            closedir(dir);
            return -1;
        }

        if (is_directory_path(path)) {
            if (delete_dir_recursive(path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            if (unlink(path) == -1) {
                perror("unlink delete file error");
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    if (rmdir(dir_path) == -1) {
        perror("rmdir face dir error");
        return -1;
    }
    return 0;
}

/* 鏍规嵁褰撳墠绠＄悊鍛橀〉绛惧垹闄ら€変腑鐨勪汉鑴稿簱鐩綍鎴栨墦鍗¤褰曟枃浠躲€?*/
static int delete_admin_item(int tab, const admin_item_t *item) {
    if (tab == ADMIN_TAB_FACES) {
        if (!is_named_face_dir(item->name) || !is_directory_path(item->path)) {
            return -1;
        }
        printf("delete face dir: %s\n", item->path);
        return delete_dir_recursive(item->path);
    }

    if (!has_suffix(item->name, ".bmp") || !is_regular_file_path(item->path)) {
        return -1;
    }
    printf("delete record file: %s\n", item->path);
    if (unlink(item->path) == -1) {
        perror("unlink record file error");
        return -1;
    }
    return 0;
}

/*
 * 閬嶅巻鎵€鏈夊凡褰曞叆浜鸿劯骞跺尮閰嶅綋鍓嶆墦鍗′汉鑴搞€? * 鍖归厤鎴愬姛鏃朵細鎶婂尮閰嶅埌鐨勫鍚嶅啓鍏?matched_name銆? */
static int match_enrolled_faces(rockx_face_feature_t *check_feature,
                                float *best_similarity,
                                char *matched_name,
                                size_t matched_name_size) {
    DIR *dir = opendir(FACE_DIR);
    if (dir == NULL) {
        perror("open face dir error");
        return -1;
    }

    int found_record = 0;
    int matched = 0;
    float best = 0.0f;
    struct dirent *dir_entry;

    while ((dir_entry = readdir(dir)) != NULL) {
        if (!is_named_face_dir(dir_entry->d_name)) {
            continue;
        }

        char subdir_path[256];
        int n = snprintf(subdir_path, sizeof(subdir_path), FACE_DIR "/%s", dir_entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(subdir_path)) {
            fprintf(stderr, "face subdir path too long: %s\n", dir_entry->d_name);
            continue;
        }
        if (!is_directory_path(subdir_path)) {
            continue;
        }

        DIR *subdir = opendir(subdir_path);
        if (subdir == NULL) {
            perror("open named face subdir error");
            continue;
        }

        struct dirent *face_entry;
        while ((face_entry = readdir(subdir)) != NULL) {
            if (!has_suffix(face_entry->d_name, ".bmp")) {
                continue;
            }

            char face_path[256];
            n = snprintf(face_path, sizeof(face_path), "%s/%s", subdir_path, face_entry->d_name);
            if (n < 0 || (size_t)n >= sizeof(face_path)) {
                fprintf(stderr, "face record path too long: %s\n", face_entry->d_name);
                continue;
            }
            if (!is_regular_file_path(face_path)) {
                continue;
            }

            found_record = 1;
            rockx_face_feature_t enrolled_feature;
            if (extract_face_feature(face_path, &enrolled_feature) == -1) {
                printf("skip invalid enrolled face: %s\n", face_path);
                continue;
            }

            float similarity = 0.0f;
            rockx_face_feature_similarity(check_feature, &enrolled_feature, &similarity);
            printf("compare %s similarity = %f\n", face_path, similarity);

            if (best == 0.0f || (similarity > 0.0f && similarity < best)) {
                best = similarity;
            }
            if (similarity > 0.0f && similarity <= FACE_MATCH_THRESHOLD) {
                if (matched_name != NULL && matched_name_size > 0) {
                    size_t dir_len = strlen(dir_entry->d_name);
                    size_t suffix_len = strlen("_face");
                    size_t copy_len = dir_len > suffix_len ? dir_len - suffix_len : dir_len;
                    if (copy_len >= matched_name_size) {
                        copy_len = matched_name_size - 1;
                    }
                    memcpy(matched_name, dir_entry->d_name, copy_len);
                    matched_name[copy_len] = '\0';
                }
                matched = 1;
                break;
            }
        }

        closedir(subdir);
        if (matched) {
            break;
        }
    }

    closedir(dir);
    if (best_similarity != NULL) {
        *best_similarity = best;
    }

    if (!found_record) {
        return -1;
    }
    return matched;
}

/* 缁樺埗杈冮暱鏂囦欢鍚嶆椂鍋氭埅鏂紝閬垮厤鏂囧瓧瓒呭嚭鍒楄〃鍖哄煙銆?*/
static void draw_short_text(int x, int y, const char *text, int max_chars, int scale,
                            unsigned char b, unsigned char g, unsigned char r) {
    char buf[96];
    int len = (int)strlen(text);
    if (len > max_chars) {
        len = max_chars;
    }
    memcpy(buf, text, (size_t)len);
    buf[len] = '\0';
    draw_text(x, y, buf, scale, b, g, r);
}

/*
 * 缁樺埗绠＄悊鍛樼晫闈€? * 椤堕儴鏄剧ず缁熻鍜岃ˉ鍗?杩斿洖鎸夐挳锛屼腑闂存樉绀轰汉鑴稿簱鎴栨墦鍗¤褰曞垪琛紝
 * 姣忚鍙充晶鎻愪緵鍒犻櫎鎸夐挳锛屽簳閮ㄦ彁渚涘垎椤垫寜閽€? */
static void draw_admin_page(int tab, int page) {
    admin_item_t items[ADMIN_PAGE_SIZE];
    int total = 0;
    int item_count = collect_admin_items(tab, page, items, ADMIN_PAGE_SIZE, &total);
    int face_count = count_face_dirs();
    int record_count = count_record_files();
    int max_page = total > 0 ? (total - 1) / ADMIN_PAGE_SIZE : 0;

    memset(g_drm.vaddr, 0, SCREEN_W * SCREEN_H * 4);
    fill_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 245, 245, 245);

    draw_utf8_text(35, 20, "绠＄悊鍛?, 2, 3, 20, 20, 20);

    char stat_text[96];
    snprintf(stat_text, sizeof(stat_text), "浜鸿劯%d 璁板綍%d", face_count, record_count);
    draw_utf8_text(250, 32, stat_text, 1, 3, 0, 80, 180);

    fill_rect(840, 20, 990, 75, 230, 230, 230);
    draw_rect_border(840, 20, 990, 75, 3, 80, 80, 80);
    draw_utf8_text(875, 32, "杩斿洖", 1, 3, 0, 0, 0);

    fill_rect(650, 20, 800, 75, 90, 180, 90);
    draw_rect_border(650, 20, 800, 75, 3, 20, 100, 20);
    draw_utf8_text(700, 32, "琛ュ崱", 1, 3, 255, 255, 255);

    fill_rect(35, 95, 210, 145, tab == ADMIN_TAB_FACES ? 80 : 255,
              tab == ADMIN_TAB_FACES ? 160 : 255,
              tab == ADMIN_TAB_FACES ? 230 : 255);
    draw_rect_border(35, 95, 210, 145, 3, 80, 80, 80);
    draw_utf8_text(67, 107, "浜鸿劯搴?, 1, 3, tab == ADMIN_TAB_FACES ? 255 : 0,
                   tab == ADMIN_TAB_FACES ? 255 : 0,
                   tab == ADMIN_TAB_FACES ? 255 : 0);

    fill_rect(230, 95, 430, 145, tab == ADMIN_TAB_RECORDS ? 80 : 255,
              tab == ADMIN_TAB_RECORDS ? 160 : 255,
              tab == ADMIN_TAB_RECORDS ? 230 : 255);
    draw_rect_border(230, 95, 430, 145, 3, 80, 80, 80);
    draw_utf8_text(258, 107, "鎵撳崱璁板綍", 1, 3, tab == ADMIN_TAB_RECORDS ? 255 : 0,
                   tab == ADMIN_TAB_RECORDS ? 255 : 0,
                   tab == ADMIN_TAB_RECORDS ? 255 : 0);

    for (int i = 0; i < item_count; i++) {
        int y1 = 165 + i * 62;
        int y2 = y1 + 52;
        fill_rect(40, y1, 820, y2, 255, 255, 255);
        draw_rect_border(40, y1, 820, y2, 2, 170, 170, 170);

        if (tab == ADMIN_TAB_FACES) {
            char line[160];
            snprintf(line, sizeof(line), "%s %d", items[i].name, items[i].count);
            draw_short_text(60, y1 + 15, line, 34, 3, 0, 0, 0);
        } else {
            draw_short_text(60, y1 + 15, items[i].name, 36, 3, 0, 0, 0);
        }

        fill_rect(850, y1, 980, y2, 80, 80, 220);
        draw_rect_border(850, y1, 980, y2, 2, 30, 30, 120);
        draw_utf8_text(885, y1 + 13, "鍒犻櫎", 1, 3, 255, 255, 255);
    }

    fill_rect(300, 545, 440, 590, 230, 230, 230);
    draw_rect_border(300, 545, 440, 590, 2, 80, 80, 80);
    draw_utf8_text(322, 553, "涓婁竴椤?, 1, 3, 0, 0, 0);

    fill_rect(560, 545, 700, 590, 230, 230, 230);
    draw_rect_border(560, 545, 700, 590, 2, 80, 80, 80);
    draw_utf8_text(582, 553, "涓嬩竴椤?, 1, 3, 0, 0, 0);

    snprintf(stat_text, sizeof(stat_text), "%d-%d", page + 1, max_page + 1);
    draw_text(475, 558, stat_text, 3, 0, 0, 0);

    DRMshowUp(g_drm_fd, &g_drm);
}

/* do_admin() 涓殑琛ュ崱鍔熻兘浼氳皟鐢?do_check()锛屽洜姝よ繖閲岄渶瑕佸墠缃０鏄庛€?*/
static int do_check(void);

/*
 * 绠＄悊鍛樻ā寮忎富寰幆銆? * 鏀寔锛? * - 浜鸿劯搴?鎵撳崱璁板綍椤电鍒囨崲
 * - 鍒犻櫎浜鸿劯鏁版嵁
 * - 鍒犻櫎鎵撳崱璁板綍
 * - 琛ュ崱
 * - 杩斿洖棣栭〉
 */
static int do_admin(void) {
    int tab = ADMIN_TAB_FACES;
    int page = 0;

    while (g_running) {
        admin_item_t items[ADMIN_PAGE_SIZE];
        int total = 0;
        int item_count = collect_admin_items(tab, page, items, ADMIN_PAGE_SIZE, &total);
        int max_page = total > 0 ? (total - 1) / ADMIN_PAGE_SIZE : 0;
        if (page > max_page) {
            page = max_page;
            continue;
        }

        draw_admin_page(tab, page);

        int x = 0;
        int y = 0;
        if (wait_touch(&x, &y) <= 0) {
            return -1;
        }

        if (in_rect(x, y, 800, 0, SCREEN_W - 1, 105)) {
            printf("admin back touched x=%d y=%d\n", x, y);
            drain_touch_events();
            return 0;
        }
        if (in_rect(x, y, 630, 0, 799, 105)) {
            printf("admin makeup check touched x=%d y=%d\n", x, y);
            drain_touch_events();
            do_check();
            drain_touch_events();
            continue;
        }
        if (in_rect(x, y, 35, 95, 210, 145)) {
            tab = ADMIN_TAB_FACES;
            page = 0;
            continue;
        }
        if (in_rect(x, y, 230, 95, 430, 145)) {
            tab = ADMIN_TAB_RECORDS;
            page = 0;
            continue;
        }
        if (in_rect(x, y, 300, 545, 440, 590)) {
            if (page > 0) {
                page--;
            }
            continue;
        }
        if (in_rect(x, y, 560, 545, 700, 590)) {
            if (page < max_page) {
                page++;
            }
            continue;
        }

        for (int i = 0; i < item_count; i++) {
            int y1 = 165 + i * 62;
            int y2 = y1 + 52;
            if (in_rect(x, y, 850, y1, 980, y2)) {
                delete_admin_item(tab, &items[i]);
                break;
            }
        }
    }

    return -1;
}

/*
 * 浜鸿劯褰曞叆椤甸潰銆? * 瀹炴椂鏄剧ず鎽勫儚澶村拰浜鸿劯妗嗭紱鐐瑰嚮褰曞叆鍚庝繚瀛樺綋鍓嶅抚銆佹彁鍙栦汉鑴哥壒寰併€? * 寮瑰嚭閿洏杈撳叆濮撳悕锛屽苟淇濆瓨鍒板搴?濮撳悕_face 鐩綍銆? */
static int do_enroll(void) {
    int x = 0;
    int y = 0;
    int has_frame = 0;
    rockx_face_feature_t enrolled_feature;

    bmp_show("./2.bmp", 0, 0);
    draw_back_button();
    DRMshowUp(g_drm_fd, &g_drm);
    printf("enroll page: live preview 600x600 without scale, touch action button to capture face\n");

    while (g_running) {
        if (update_face_preview(ENROLL_RAW_BMP, ENROLL_BOX_BMP) == 0) {
            has_frame = 1;
        }

        int touch_ret = poll_touch(&x, &y);
        if (touch_ret < 0) {
            return -1;
        }

        if (touch_ret == 1) {
            if (in_rect(x, y, BTN_BACK_X1, BTN_BACK_Y1, BTN_BACK_X2, BTN_BACK_Y2)) {
                return 0;
            }
            if (!in_rect(x, y, BTN_ACTION_X1, BTN_ACTION_Y1, BTN_ACTION_X2, BTN_ACTION_Y2)) {
                continue;
            }

            if (!has_frame) {
                printf("no preview frame ready\n");
                show_result_then_restore(ENROLL_FAIL_BMP, "./2.bmp");
                continue;
            }

            save_rgb_bmp(g_preview_rgb, ENROLL_RAW_BMP);
            if (extract_face_feature_from_rgb(g_preview_rgb, &enrolled_feature) == 0) {
                char face_name[MAX_FACE_NAME_LEN + 1];
                if (input_face_name(face_name, sizeof(face_name)) == 0 &&
                    save_enrolled_face_image(ENROLL_RAW_BMP, face_name) == 0) {
                    printf("face enroll success: %s\n", face_name);
                    show_result_then_restore(ENROLL_SUCCESS_BMP, "./2.bmp");
                } else {
                    printf("face name input or save failed\n");
                    show_result_then_restore(ENROLL_FAIL_BMP, "./2.bmp");
                }
            } else {
                printf("face enroll failed\n");
                show_result_then_restore(ENROLL_FAIL_BMP, "./2.bmp");
            }
            has_frame = 0;
            continue;
        }

        usleep(50000);
    }

    return -1;
}

/*
 * 鑰冨嫟鎵撳崱椤甸潰銆? * 瀹炴椂鏄剧ず鎽勫儚澶村拰浜鸿劯妗嗭紱鐐瑰嚮鎵撳崱鍚庢彁鍙栧綋鍓嶄汉鑴哥壒寰侊紝
 * 涓?face 鐩綍涓墍鏈夊凡褰曞叆浜鸿劯鍖归厤锛屽苟淇濆瓨鎴愬姛璁板綍銆? */
static int do_check(void) {
    int x = 0;
    int y = 0;
    int has_frame = 0;
    rockx_face_feature_t check_feature;
    float best_similarity = 0.0f;
    char matched_name[MAX_FACE_NAME_LEN + 1];

    bmp_show("./3.bmp", 0, 0);
    draw_back_button();
    DRMshowUp(g_drm_fd, &g_drm);
    printf("check page: live preview 640x480 without scale, touch action button to capture face\n");

    while (g_running) {
        if (update_face_preview(CHECK_RAW_BMP, CHECK_BOX_BMP) == 0) {
            has_frame = 1;
        }

        int touch_ret = poll_touch(&x, &y);
        if (touch_ret < 0) {
            return -1;
        }

        if (touch_ret == 1) {
            if (in_rect(x, y, BTN_BACK_X1, BTN_BACK_Y1, BTN_BACK_X2, BTN_BACK_Y2)) {
                return 0;
            }
            if (!in_rect(x, y, BTN_ACTION_X1, BTN_ACTION_Y1, BTN_ACTION_X2, BTN_ACTION_Y2)) {
                continue;
            }

            if (!has_frame) {
                printf("no preview frame ready\n");
                show_result_then_restore(CHECK_FAIL_BMP, "./3.bmp");
                continue;
            }

            save_rgb_bmp(g_preview_rgb, CHECK_RAW_BMP);
            if (extract_face_feature_from_rgb(g_preview_rgb, &check_feature) == -1) {
                printf("check capture failed\n");
                show_result_then_restore(CHECK_FAIL_BMP, "./3.bmp");
                has_frame = 0;
                continue;
            }

            matched_name[0] = '\0';
            int match_ret = match_enrolled_faces(&check_feature,
                                                 &best_similarity,
                                                 matched_name,
                                                 sizeof(matched_name));
            printf("best similarity = %f\n", best_similarity);
            if (match_ret == 1) {
                printf("attendance check success: %s\n", matched_name);
                save_check_record_image(CHECK_RAW_BMP, matched_name);
                show_check_success_with_name(matched_name);
            } else {
                printf("attendance check failed\n");
                show_result_then_restore(CHECK_FAIL_BMP, "./3.bmp");
            }
            has_frame = 0;
            continue;
        }

        usleep(50000);
    }

    return -1;
}

/* 绋嬪簭閫€鍑哄墠閲婃斁鎵€鏈夌‖浠惰祫婧愬拰 RockX 妯″瀷璧勬簮銆?*/
static void cleanup(void) {
    if (g_video_fd >= 0) {
        free_video(g_video_fd);
        g_video_fd = -1;
    }

    if (g_touch_fd >= 0) {
        close(g_touch_fd);
        g_touch_fd = -1;
    }

    if (g_drm_fd >= 0 && g_drm_ready) {
        DRMfreeResources(g_drm_fd, &g_drm);
    }

    if (g_drm_fd >= 0) {
        close(g_drm_fd);
        g_drm_fd = -1;
    }

    if (g_face_det_ready) {
        rockx_destroy(g_face_det_handle);
        g_face_det_ready = 0;
    }
    if (g_face_5landmarks_ready) {
        rockx_destroy(g_face_5landmarks_handle);
        g_face_5landmarks_ready = 0;
    }
    if (g_face_recognize_ready) {
        rockx_destroy(g_face_recognize_handle);
        g_face_recognize_ready = 0;
    }
}

/*
 * 绋嬪簭鍏ュ彛銆? * 鍒濆鍖栫洰褰曘€丏RM銆佽Е鎽搞€佹憚鍍忓ご銆丷ockX 鍚庤繘鍏ラ椤靛惊鐜紝
 * 鏍规嵁瑙︽懜鍧愭爣鍒囨崲鍒扮鐞嗗憳銆佸綍鍏ユ垨鎵撳崱椤甸潰銆? */
int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (ensure_face_dir() == -1) {
        return -1;
    }
    if (ensure_record_dir() == -1) {
        return -1;
    }

    g_drm_fd = open("/dev/dri/card0", O_RDWR);
    if (g_drm_fd == -1) {
        perror("open drm file error");
        return -1;
    }
    DRMinit(g_drm_fd);
    DRMcreateFB(g_drm_fd, &g_drm);
    g_drm_ready = 1;

    g_touch_fd = init_touch();
    if (g_touch_fd == -1) {
        cleanup();
        return -1;
    }

    g_video_fd = init_video();
    if (g_video_fd == -1) {
        cleanup();
        return -1;
    }

    if (face_init() == -1) {
        cleanup();
        return -1;
    }

    while (g_running) {
        int x = 0;
        int y = 0;

        bmp_show("./1.bmp", 0, 0);
        if (wait_touch(&x, &y) <= 0) {
            break;
        }

        if (in_rect(x, y, BTN_ADMIN_X1, BTN_ADMIN_Y1, BTN_ADMIN_X2, BTN_ADMIN_Y2)) {
            drain_touch_events();
            do_admin();
            drain_touch_events();
            continue;
        } else if (in_rect(x, y, BTN_ENROLL_X1, BTN_ENROLL_Y1, BTN_ENROLL_X2, BTN_ENROLL_Y2)) {
            drain_touch_events();
            do_enroll();
            drain_touch_events();
        } else if (in_rect(x, y, BTN_CHECK_X1, BTN_CHECK_Y1, BTN_CHECK_X2, BTN_CHECK_Y2)) {
            drain_touch_events();
            do_check();
            drain_touch_events();
        }
    }

    cleanup();
    return 0;
}

