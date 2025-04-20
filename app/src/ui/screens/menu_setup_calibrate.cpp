#include <stdio.h>
#include "thermostat.hpp"
#include "interface.hpp"
#include "../ui.h"
#include "menu_internal.h"

static int align_state = 0;
static const lv_point_t align_target_pts[6] = { {11, 13}, {220, 20}, {60, 167}, {180, 155}, {13, 300}, {225, 295} };
static lv_point_t align_read_pts[6];
static lv_obj_t *img;

calibration_t calibrate(const lv_point_t *t, const lv_point_t *s)
{
    // t = read points
    // s = screen points
    float a = 0, b = 0, c = 0, d = 0, e = 0;
    float X1 = 0, X2 = 0, X3 = 0;
    float Y1 = 0, Y2 = 0, Y3 = 0;
    for (int k = 0; k < 6; k++) {
        a += t[k].x * t[k].x;
        b += t[k].y * t[k].y;
        c += t[k].x * t[k].y;
        d += t[k].x;
        e += t[k].y;
        X1 += t[k].x * s[k].x;
        X2 += t[k].y * s[k].x;
        X3 +=          s[k].x;
        Y1 += t[k].x * s[k].y;
        Y2 += t[k].y * s[k].y;
        Y3 +=          s[k].y;
    }
    float delta = 6 * (a * b - c * c) + 2 * c * d * e - a * e * e - b * d * d;

    float delta_x1 = 6 * (X1 * b - X2 * c) + e * (X2 * d - X1 * e) + X3 * (c * e - b * d);
    float delta_x2 = 6 * (X2 * a - X1 * c) + d * (X1 * e - X2 * d) + X3 * (c * d - a * e);
    float delta_x3 = X3 * (a * b - c * c)  + X1 * (c * e - b * d)  + X2 * (c * d - a * e);

    float delta_y1 = 6 * (Y1 * b - Y2 * c) + e * (Y2 * d - Y1 * e) + Y3 * (c * e - b * d);
    float delta_y2 = 6 * (Y2 * a - Y1 * c) + d * (Y1 * e - Y2 * d) + Y3 * (c * d - a * e);
    float delta_y3 = Y3 * (a * b - c * c)  + Y1 * (c * e - b * d)  + Y2 * (c * d - a * e);

    calibration_t cal;
    cal.alpha_x = delta_x1 / delta;
    cal.beta_x  = delta_x2 / delta;
    cal.delta_x = delta_x3 / delta;
    cal.alpha_y = delta_y1 / delta;
    cal.beta_y  = delta_y2 / delta;
    cal.delta_y = delta_y3 / delta;

    printf("alphaX: %f betaX: %f deltaX: %f\n", cal.alpha_x, cal.beta_x, cal.delta_x);
    printf("alphaY: %f betaY: %f deltaY: %f\n", cal.alpha_y, cal.beta_y, cal.delta_y);
    return cal;
}

lv_point_t calculate_point(const lv_point_t touchp, calibration_t *cal)
{
    lv_point_t p;
    p.x = cal->alpha_x * touchp.x + cal->beta_x * touchp.y + cal->delta_x;
    p.y = cal->alpha_y * touchp.x + cal->beta_y * touchp.y + cal->delta_y;
    return p;
}

static void my_timer(lv_timer_t * timer)
{
    transition_leaf_to_menu();
}

static void align_cb(lv_event_t *e)
{
    lv_point_t rawp;
    getRawPoint(&rawp.x, &rawp.y);
    if (align_state < 6) {
        align_read_pts[align_state] = rawp;
    }
    printf("Got point %d: %d x %d\n", align_state, align_read_pts[align_state].x, align_read_pts[align_state].y);
    align_state++;

    if (img) {
        lv_obj_del(img);
        img = NULL;
    }
    if (align_state < 6) {

        img = draw_calibration_point(scr_leaf, align_target_pts[align_state].x, align_target_pts[align_state].y,
                                    CALIB_POINT_X, lv_color_white());
    } else if (align_state == 6) {
        lv_obj_clean(scr_leaf);
        calibration_t calibration = calibrate(align_read_pts, align_target_pts);
        lv_obj_t *label = lv_label_create(scr_leaf);
        lv_label_set_text_static(label, "press + to save\nanywhere else to cancel");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);
        draw_calibration_point(scr_leaf, 120, 160, CALIB_POINT_PLUS, lv_color_hex(0xff0000));
        for (int i = 0; i < 6; i++) {
            draw_calibration_point(scr_leaf, align_target_pts[i].x, align_target_pts[i].y,
                CALIB_POINT_X, lv_color_white());
            lv_point_t calc_pt = calculate_point(align_read_pts[i], &calibration);
            printf("(%dx%d) -> (%dx%d)\n", align_target_pts[i].x, align_target_pts[i].y, calc_pt.x, calc_pt.y);
            draw_calibration_point(scr_leaf, calc_pt.x, calc_pt.y, CALIB_POINT_CIRCLE, lv_color_hex(0x0000ff));
        }
    } else {
        lv_obj_clean(scr_leaf);
        lv_obj_remove_event_cb(scr_leaf, align_cb);
        calibration_t calibration = calibrate(align_read_pts, align_target_pts);
        lv_point_t p = calculate_point(rawp, &calibration);
        lv_obj_t * label = lv_label_create(scr_leaf);
        if (p.x > 120 - 25 && p.x < 120 + 25 && p.y > 160 -25 && p.y < 160 + 25) {
            lv_label_set_text(label, "Success");
            updateCalibration(&calibration);
        } else {
            lv_label_set_text(label, "Aborted");
        }
        lv_obj_center(label);
        lv_timer_t * timer = lv_timer_create(my_timer, 1000, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }
}

void menuCalibrateScreen(const char *title) {
    lv_obj_clean(scr_leaf);
    lv_scr_load_anim(scr_leaf, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    lv_obj_t *scr = scr_leaf;

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text_static(label, "press each x");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    align_state = 0;
    img = draw_calibration_point(scr, align_target_pts[0].x, align_target_pts[0].y,
                                           CALIB_POINT_X, lv_color_white());

    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, align_cb, LV_EVENT_CLICKED, img);
    const lv_point_t pts[6] = { {110, 130}, {2200, 200}, {600, 1670}, {1800, 1550}, {130, 3000}, {2250, 2950} };

    calibrate(pts, align_target_pts);
}
