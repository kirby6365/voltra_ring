#include "ui.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "hw/battery.h"
#include "hw/button.h"
#include "hw/haptics.h"
#include "hw/knob.h"
#include "hw/power.h"
#include "ui/knob_step.h"
#include "voltra/voltra_client.h"

// Arduino.h defines a global `class Client` (the network stream base), so the
// Voltra client gets an alias rather than a plain using-declaration.
using VClient = voltra::Client;
using voltra::ConnState;
using voltra::DeviceState;
using voltra::FoundDevice;

namespace {

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
#define C_BG       lv_color_hex(0x000000)
#define C_CARD     lv_color_hex(0x111827)
#define C_TRACK    lv_color_hex(0x1f2937)
#define C_TEXT     lv_color_hex(0xf8fafc)
#define C_MUTED    lv_color_hex(0x94a3b8)
#define C_UNLOADED lv_color_hex(0x3b82f6)
#define C_LOADED   lv_color_hex(0xc6fe28)
// Unloaded, as on the Voltra: the weight in a muted green and the track in grey.
#define C_IDLE_NUM  lv_color_hex(0x29320d)
#define C_IDLE_RING lv_color_hex(0x6b7280)
#define C_CHAINS   lv_color_hex(0xa78bfa)
#define C_ECC      lv_color_hex(0xf472b6)
#define C_WARN     lv_color_hex(0xf59e0b)
#define C_DANGER   lv_color_hex(0xef4444)
// Same colours as text for label recolour markup ("#rrggbb text#").
#define C_BT_HEX    "3b82f6"
#define C_WHITE_HEX "f8fafc"

// Accessory icons in font_icons_26 (drawn by tools/gen_icons.py after the Voltra's own).
#define ICON_ECCENTRIC "\xEE\x80\x80"   // U+E000
#define ICON_CHAINS    "\xEE\x80\x81"   // U+E001
#define ICON_INVERSE   "\xEE\x80\x82"   // U+E002
#define ICON_MOUNTAIN  "\xEE\x80\x83"   // U+E003
#define ICON_SETTINGS  "\xEE\x80\x84"   // U+E004

// ---------------------------------------------------------------------------
// Layout: 172 x 320 portrait. The weight track (a rounded rectangle) sits at the top with
// the number inside it, and the touch controls go in the space underneath. Offsets are
// from the screen centre.
// ---------------------------------------------------------------------------
constexpr int SCR_W = 172;
constexpr int SCR_H = 320;
constexpr int TRACK_WIDTH = 166;
constexpr int TRACK_HEIGHT = 150;
constexpr int TRACK_LINE = 10;
constexpr int TRACK_TOP = 28;
constexpr int TRACK_DY = TRACK_TOP + TRACK_HEIGHT / 2 - SCR_H / 2;   // track centre
constexpr int TRACK_INNER_W = TRACK_WIDTH - 2 * TRACK_LINE - 8;      // room inside it
constexpr int TRACK_INNER_H = TRACK_HEIGHT - 2 * TRACK_LINE - 8;
constexpr int ROW_STATE_DY = 34;     // "Press to Load" etc., under the track
constexpr int ROW_INFO_DY = 53;      // reps / force on the cable
constexpr int ROW_CHIPS_DY = 84;     // active accessories
constexpr int ROW_BOTTOM_DY = 132;   // presets either side of the settings button

// Knob stepping (fine/coarse selection and snapping) lives in ui/knob_step.h.
using knobstep::apply_step;
constexpr int STEP_COARSE_LB = knobstep::STEP_COARSE_LB;

constexpr uint32_t SEND_DEBOUNCE_MS = 150;
constexpr uint32_t EDIT_HOLD_MS = 900;
// Long enough to cover the client re-issuing the command while the Voltra steps
// through its intermediate state, so the screen says "LOADING..." rather than
// flicking back to unloaded mid-sequence.
constexpr uint32_t TOGGLE_PENDING_MS = 5000;
constexpr uint32_t AUTO_LOAD_PENDING_MS = 4000;

enum class Screen { Main, Settings, AdjustChains, AdjustEcc, Connect };

/**
 * Chains, inverse chains and mountain are one accessory in three styles: the Voltra
 * keeps a single amount (the chains percentage) and two flags pick the style, so only
 * one of them can be on at a time.
 */
enum class ChainStyle { Chains, Inverse, Mountain };

/** Rows of the settings menu, stored in each row's user data. */
enum SettingsRow : intptr_t {
    ROW_CHAINS = 1,
    ROW_INVERSE,
    ROW_ECCENTRIC,
    ROW_MOUNTAIN,
    ROW_CLOSE,
};

/** What a weight track shows: a value in a range, filled in one colour. */
struct Track {
    int min = 0, max = 100, value = 0;
    bool symmetric = false;   // fill from the middle of the range (top centre), as for eccentric
    lv_color_t colour = {};
};

struct Ui {
    // main
    lv_obj_t *scr_main = nullptr;
    lv_obj_t *track_weight = nullptr;
    Track track_weight_st;
    lv_obj_t *btn_top = nullptr;
    lv_obj_t *lbl_top = nullptr;
    lv_obj_t *lbl_reps = nullptr;
    // big set / rep counters, shown instead of the bubbles while a set is under way
    lv_obj_t *lbl_set_cap = nullptr, *lbl_set_num = nullptr;
    lv_obj_t *lbl_rep_cap = nullptr, *lbl_rep_num = nullptr;
    // before the first rep: three dots pulsing left to right, as on the Voltra
    lv_obj_t *rep_dots = nullptr;
    lv_obj_t *rep_dot[3] = {};
    bool rep_dots_running = false;
    lv_obj_t *btn_center = nullptr;
    lv_obj_t *lbl_weight = nullptr;
    lv_obj_t *lbl_unit = nullptr;
    lv_obj_t *row_weight = nullptr;      // the weight number and its unit
    lv_obj_t *lbl_link = nullptr;        // "Not Connected" / "Connecting..." in place of the weight
    lv_obj_t *lbl_state = nullptr;
    lv_obj_t *btn_gear = nullptr;
    lv_obj_t *lbl_gear = nullptr;
    // active accessories in a row above the settings button: eccentric left, chain style right
    struct Chip {
        lv_obj_t *btn = nullptr;
        lv_obj_t *icon = nullptr;
        lv_obj_t *primary = nullptr;     // in the unit the Voltra is set to show first
    };
    Chip chip_ecc, chip_chain;

    // Two weight presets either side of the settings button, kept on the remote only (NVS).
    struct Preset {
        lv_obj_t *btn = nullptr;
        lv_obj_t *value = nullptr;
        lv_obj_t *unit = nullptr;
        int lb = 0;   // 0 = empty
    };
    Preset presets[2];
    lv_obj_t *lbl_kbat = nullptr;

    // settings menu: one bubble per accessory
    lv_obj_t *scr_settings = nullptr;
    struct Bubble {
        lv_obj_t *btn = nullptr;
        lv_obj_t *icon = nullptr;
        lv_obj_t *title = nullptr;
        lv_obj_t *value = nullptr;
    };
    Bubble bub_ecc, bub_chains, bub_mountain, bub_inverse;
    lv_obj_t *btn_settings_done = nullptr;

    // adjust
    lv_obj_t *scr_adjust = nullptr;
    lv_obj_t *track_adj = nullptr;
    Track track_adj_st;
    lv_obj_t *lbl_adj_title = nullptr;
    lv_obj_t *btn_adj_center = nullptr;
    lv_obj_t *lbl_adj_val = nullptr;
    lv_obj_t *lbl_adj_unit = nullptr;
    lv_obj_t *lbl_adj_range = nullptr;
    lv_obj_t *btn_done = nullptr;

    // power-off countdown, over whatever screen is showing
    lv_obj_t *power_overlay = nullptr;
    lv_obj_t *lbl_power_title = nullptr;
    lv_obj_t *lbl_power_count = nullptr;
    lv_obj_t *lbl_power_hint = nullptr;

    // connect
    lv_obj_t *scr_connect = nullptr;
    lv_obj_t *lbl_conn_status = nullptr;
    lv_obj_t *spinner = nullptr;
    lv_obj_t *list = nullptr;
    lv_obj_t *btn_close = nullptr;

    Screen screen = Screen::Main;
    Screen adj_return = Screen::Settings;   // where the dial screen's Done goes back to

    // local (dial-edited) values
    int weight = 45;
    // Accessory amounts in the unit the Voltra is set to (see lb_mode()): percent of the
    // base weight, or pounds.
    int chains = 0;            // shared by chains, inverse chains and mountain
    int ecc = 0;
    int last_display = -2;     // Voltra lb/% setting the values above were taken in
    ChainStyle style = ChainStyle::Chains;       // what the amount currently drives
    ChainStyle adj_style = ChainStyle::Chains;   // which style the dial screen edits
    bool w_dirty = false, c_dirty = false, e_dirty = false;
    bool style_dirty = false;  // send the style flags along with the next amount
    uint32_t w_changed = 0, c_changed = 0, e_changed = 0;
    uint32_t editing_until = 0;
    uint32_t toggle_pending_until = 0;
    bool toggle_target_loaded = false;
    // Auto load was just requested: show it until the Voltra reports its own progress.
    uint32_t auto_load_pending_until = 0;
    // A set is under way: loaded and the cable has moved since the last rest.
    bool set_active = false;

    knobstep::RateTracker knob_rate;

    uint32_t last_state_version = 0xFFFFFFFF;
    uint32_t last_dev_version = 0xFFFFFFFF;
    uint32_t last_kbat_ms = 0;
    DeviceState st;
    std::vector<FoundDevice> devs;
} ui;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }


/** Weight ceiling: the Voltra's own max-weight setting (230 lb until it is read). */
int weight_max()
{
    return ui.st.max_weight > voltra::MIN_TARGET_LB ? ui.st.max_weight : voltra::DEFAULT_MAX_TARGET_LB;
}

/**
 * The Voltra's own lb/% setting decides the unit accessories are dialled and stored in:
 * it keeps the chosen unit authoritative and derives the other, so the knob works in the
 * same unit and shows the other alongside.
 */
bool lb_mode() { return ui.st.accessory_lb(); }
const char *accessory_unit() { return ui.st.accessory_unit(); }

/** Pound equivalent of a percentage of the current base weight, for display only. */
int pct_to_lb(int pct) { return (ui.weight * pct) / 100; }

/** Accessory amount (in the Voltra's unit) as percent. */
int amount_pct(int v)
{
    if (!lb_mode()) return v;
    if (ui.weight <= 0) return 0;
    return (v * 100 + (v >= 0 ? ui.weight / 2 : -ui.weight / 2)) / ui.weight;
}

// Limits are percentages on the device; in pound mode they apply to the current base weight.
int chains_max() { return lb_mode() ? pct_to_lb(ui.st.max_chains_pct) : ui.st.max_chains_pct; }
int ecc_max() { return lb_mode() ? pct_to_lb(ui.st.max_ecc_pct) : ui.st.max_ecc_pct; }

/** An amount in the Voltra's unit, e.g. "25%", "+6 lb". */
void format_amount(char *buf, size_t n, int v, bool signed_value)
{
    if (lb_mode()) snprintf(buf, n, signed_value ? "%+d lb" : "%d lb", v);
    else snprintf(buf, n, signed_value ? "%+d%%" : "%d%%", v);
}

/** Fill a chip's value in the Voltra's unit (the dial screen shows the other one too). */
void set_chip_values(Ui::Chip &c, int v, bool signed_value)
{
    char buf[12];
    format_amount(buf, sizeof(buf), v, signed_value);
    lv_label_set_text(c.primary, buf);
}

/** Amount a given chain style is set to: the shared amount if it is the live style, else 0. */
int style_amount(ChainStyle s) { return ui.style == s ? ui.chains : 0; }

const char *style_name(ChainStyle s)
{
    switch (s) {
        case ChainStyle::Inverse: return "Inverse Chains";
        case ChainStyle::Mountain: return "Mountain";
        default: return "Chains";
    }
}

lv_color_t style_colour(ChainStyle s) { return s == ChainStyle::Mountain ? C_WARN : C_CHAINS; }

/** Style the device flags describe. Both flags set is not a state the Voltra's UI makes. */
ChainStyle device_style(const DeviceState &st)
{
    if (st.mountain == 1) return ChainStyle::Mountain;
    if (st.inverse_chains == 1) return ChainStyle::Inverse;
    return ChainStyle::Chains;
}

void show(Screen s)
{
    ui.screen = s;
    lv_obj_t *scr = ui.scr_main;
    if (s == Screen::Settings) scr = ui.scr_settings;
    if (s == Screen::AdjustChains || s == Screen::AdjustEcc) scr = ui.scr_adjust;
    if (s == Screen::Connect) scr = ui.scr_connect;
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 120, 0, false);
}

// ---------------------------------------------------------------------------
// Widget helpers
// ---------------------------------------------------------------------------
lv_obj_t *make_screen()
{
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

// ---------------------------------------------------------------------------
// Weight track: a rounded rectangle round the number, open in the middle of the bottom
// edge, filling clockwise from the bottom-left end as the Voltra's ring does. Drawn as a
// polyline along its centre line (each corner sampled into short segments).
// ---------------------------------------------------------------------------
constexpr int TRACK_R = 20;          // corner radius of the centre line
constexpr int TRACK_GAP = 28;        // half the opening in the bottom edge
constexpr int CORNER_STEPS = 9;
constexpr int TRACK_MAX_PTS = 2 + 4 * (CORNER_STEPS + 1);

struct PathPt {
    float x, y;
};

/** The track's centre line in object-local coordinates, start to end. Returns the point count. */
int track_path(int w, int h, PathPt *out)
{
    const float x0 = TRACK_LINE / 2.0f, x1 = w - TRACK_LINE / 2.0f;
    const float y0 = TRACK_LINE / 2.0f, y1 = h - TRACK_LINE / 2.0f;
    const float r = TRACK_R;
    int n = 0;
    out[n++] = {w / 2.0f - TRACK_GAP, y1};
    // Corners clockwise from the bottom-left. Angles in degrees with y growing downwards:
    // 90 points down, 180 left, 270 up, 0 right.
    const struct { float cx, cy, a0; } corners[] = {
        {x0 + r, y1 - r, 90}, {x0 + r, y0 + r, 180}, {x1 - r, y0 + r, 270}, {x1 - r, y1 - r, 0},
    };
    for (const auto &c : corners) {
        for (int i = 0; i <= CORNER_STEPS; i++) {
            const float a = (c.a0 + 90.0f * i / CORNER_STEPS) * 3.14159265f / 180.0f;
            out[n++] = {c.cx + r * cosf(a), c.cy + r * sinf(a)};
        }
    }
    out[n++] = {w / 2.0f + TRACK_GAP, y1};
    return n;
}

/** Draw the stretch of the path from s0 to s1 px along it. */
void draw_track_span(lv_draw_ctx_t *ctx, const lv_area_t &box, const PathPt *p, const float *along, int n,
                     float s0, float s1, lv_color_t colour)
{
    if (s1 - s0 < 0.5f) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = colour;
    d.width = TRACK_LINE;
    d.round_start = 1;   // round caps also fill the joints between segments
    d.round_end = 1;
    for (int i = 1; i < n; i++) {
        const float a = along[i - 1], b = along[i];
        if (b <= s0 || a >= s1 || b - a <= 0) continue;
        const float t0 = (std::max(a, s0) - a) / (b - a);
        const float t1 = (std::min(b, s1) - a) / (b - a);
        const PathPt &u = p[i - 1], &v = p[i];
        lv_point_t q0 = {(lv_coord_t)lroundf(box.x1 + u.x + (v.x - u.x) * t0),
                         (lv_coord_t)lroundf(box.y1 + u.y + (v.y - u.y) * t0)};
        lv_point_t q1 = {(lv_coord_t)lroundf(box.x1 + u.x + (v.x - u.x) * t1),
                         (lv_coord_t)lroundf(box.y1 + u.y + (v.y - u.y) * t1)};
        lv_draw_line(ctx, &d, &q0, &q1);
    }
}

void track_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const Track *t = static_cast<const Track *>(lv_obj_get_user_data(obj));
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t box;
    lv_obj_get_coords(obj, &box);

    PathPt p[TRACK_MAX_PTS];
    float along[TRACK_MAX_PTS];
    const int n = track_path(lv_area_get_width(&box), lv_area_get_height(&box), p);
    along[0] = 0;
    for (int i = 1; i < n; i++) along[i] = along[i - 1] + hypotf(p[i].x - p[i - 1].x, p[i].y - p[i - 1].y);
    const float len = along[n - 1];

    auto pos = [&](int v) {
        const int span = t->max - t->min;
        if (span <= 0) return 0.0f;
        return len * (float)(clampi(v, t->min, t->max) - t->min) / (float)span;
    };
    draw_track_span(ctx, box, p, along, n, 0, len, C_TRACK);
    float a = 0, b = pos(t->value);
    if (t->symmetric) {
        a = len / 2;   // the middle of the range sits at the top centre
        if (b < a) std::swap(a, b);
    }
    draw_track_span(ctx, box, p, along, n, a, b, t->colour);
}

lv_obj_t *make_track(lv_obj_t *parent, Track &t, lv_color_t colour)
{
    t.colour = colour;
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, TRACK_WIDTH, TRACK_HEIGHT);
    lv_obj_align(o, LV_ALIGN_CENTER, 0, TRACK_DY);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(o, &t);
    lv_obj_add_event_cb(o, track_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

/** Set a track's range, value and colour; redraws only when something changed. */
void track_set(lv_obj_t *o, int min, int max, int value, lv_color_t colour, bool symmetric = false)
{
    Track *t = static_cast<Track *>(lv_obj_get_user_data(o));
    if (t->min == min && t->max == max && t->value == value && t->symmetric == symmetric &&
        t->colour.full == colour.full) {
        return;
    }
    t->min = min;
    t->max = max;
    t->value = value;
    t->symmetric = symmetric;
    t->colour = colour;
    lv_obj_invalidate(o);
}

lv_obj_t *make_flat_button(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    return b;
}

lv_obj_t *make_pill_button(lv_obj_t *parent, int w, int h, lv_color_t border, const char *text, lv_obj_t **out_label)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_CARD, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, border, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, border, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = make_label(b, &font_poppins_14, C_TEXT, text);
    lv_obj_center(l);
    if (out_label) *out_label = l;
    return b;
}

/**
 * The Done / Close button of a sub-screen, across the bottom. Sized for a thumb: the
 * whole pill is the touch target, plus a margin around it.
 */
lv_obj_t *make_action_button(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = nullptr;
    lv_obj_t *b = make_pill_button(parent, 140, 44, C_MUTED, text, &label);
    lv_obj_set_style_text_font(label, &font_poppins_18, 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_ext_click_area(b, 8);
    return b;
}

/** Screen title along the top edge. */
lv_obj_t *make_title(lv_obj_t *parent, const char *text, lv_color_t colour)
{
    lv_obj_t *l = make_label(parent, &font_poppins_18, colour, text);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 6);
    return l;
}

/**
 * Big number with its small unit underneath (the track is too narrow for them side by
 * side). The number is centred in a fixed width so changing digits do not move the unit.
 */
lv_obj_t *make_value_stack(lv_obj_t *parent, const lv_font_t *big, lv_obj_t **num, lv_obj_t **unit)
{
    // The big font's line box has ~20 px empty above the digits and base_line below.
    const int digits_bottom = big->line_height - big->base_line;
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, TRACK_INNER_W, digits_bottom + 28);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    *num = make_label(box, big, C_TEXT, "--");
    lv_obj_set_width(*num, TRACK_INNER_W);
    lv_obj_set_style_text_align(*num, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(*num, LV_ALIGN_TOP_MID, 0, 0);
    *unit = make_label(box, &font_poppins_18, C_MUTED, "lb");
    lv_obj_align(*unit, LV_ALIGN_TOP_MID, 0, digits_bottom);
    return box;
}

// ---------------------------------------------------------------------------
// Sending edits to the device
// ---------------------------------------------------------------------------
void mark_weight_changed()
{
    uint32_t now = millis();
    ui.w_dirty = true;
    ui.w_changed = now;
    ui.editing_until = now + EDIT_HOLD_MS;
}

void mark_chains_changed();
void mark_ecc_changed();

/**
 * Set the target weight from the knob side (dial or preset). It is sent after the usual
 * debounce, and chains / eccentric are pulled inside the limits the new weight allows.
 * @return false if the weight did not change.
 */
bool set_weight(int lb)
{
    const int v = clampi(lb, voltra::MIN_TARGET_LB, weight_max());
    if (v == ui.weight) return false;
    ui.weight = v;
    mark_weight_changed();
    int cm = chains_max();
    if (ui.chains > cm) { ui.chains = cm; mark_chains_changed(); }
    int em = ecc_max();
    if (ui.ecc > em) { ui.ecc = em; mark_ecc_changed(); }
    if (ui.ecc < -em) { ui.ecc = -em; mark_ecc_changed(); }
    return true;
}

void mark_chains_changed()
{
    uint32_t now = millis();
    ui.c_dirty = true;
    ui.c_changed = now;
    ui.editing_until = now + EDIT_HOLD_MS;
}

void mark_ecc_changed()
{
    uint32_t now = millis();
    ui.e_dirty = true;
    ui.e_changed = now;
    ui.editing_until = now + EDIT_HOLD_MS;
}

void flush_pending(uint32_t now)
{
    VClient &c = VClient::instance();
    if (ui.w_dirty && now - ui.w_changed >= SEND_DEBOUNCE_MS) {
        ui.w_dirty = false;
        if (ui.st.connected()) c.setWeight(ui.weight);
    }
    if (ui.c_dirty && now - ui.c_changed >= SEND_DEBOUNCE_MS) {
        ui.c_dirty = false;
        if (ui.st.connected()) {
            // the client writes the flags before the amount
            if (ui.style_dirty) {
                c.setInverseChains(ui.style == ChainStyle::Inverse);
                c.setMountain(ui.style == ChainStyle::Mountain);
            }
            c.setChains(ui.chains, lb_mode());
        }
        ui.style_dirty = false;
    }
    if (ui.e_dirty && now - ui.e_changed >= SEND_DEBOUNCE_MS) {
        ui.e_dirty = false;
        if (ui.st.connected()) c.setEccentric(ui.ecc, lb_mode());
    }
}

// ---------------------------------------------------------------------------
// Main screen
// ---------------------------------------------------------------------------
void set_obj_hidden(lv_obj_t *o, bool hidden)
{
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void rep_dot_opa_cb(void *dot, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)dot, (lv_opa_t)v, 0); }

/**
 * Show or hide the waiting-for-first-rep dots. Each dot fades up and back down; the
 * starts are staggered so the pulse travels left to right. Animations only run while
 * the dots are on screen.
 */
void set_rep_dots(bool show)
{
    if (show == ui.rep_dots_running) return;
    ui.rep_dots_running = show;
    set_obj_hidden(ui.rep_dots, !show);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *d = ui.rep_dot[i];
        lv_anim_del(d, rep_dot_opa_cb);
        lv_obj_set_style_bg_opa(d, LV_OPA_30, 0);
        if (!show) continue;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, d);
        lv_anim_set_exec_cb(&a, rep_dot_opa_cb);
        lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
        lv_anim_set_time(&a, 300);
        lv_anim_set_playback_time(&a, 300);
        lv_anim_set_repeat_delay(&a, 300);   // one cycle = 900 ms, a third per dot
        lv_anim_set_delay(&a, i * 300);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

// ---------------------------------------------------------------------------
// Weight presets
// ---------------------------------------------------------------------------
void refresh_main();

constexpr const char *PRESET_NS = "knob";
const char *const PRESET_KEYS[2] = {"preset0", "preset1"};

void load_presets()
{
    Preferences p;
    if (!p.begin(PRESET_NS, true)) return;   // namespace not created yet: all empty
    for (int i = 0; i < 2; i++) ui.presets[i].lb = p.getUShort(PRESET_KEYS[i], 0);
    p.end();
}

void save_preset(int i)
{
    Preferences p;
    if (!p.begin(PRESET_NS, false)) return;
    p.putUShort(PRESET_KEYS[i], (uint16_t)ui.presets[i].lb);
    p.end();
}

/** Stored weight, or "+" when empty; outlined in white when it matches the current weight. */
void refresh_presets()
{
    for (auto &pr : ui.presets) {
        if (pr.lb > 0) {
            lv_label_set_text_fmt(pr.value, "%d", pr.lb);
            lv_obj_set_style_text_color(pr.value, C_TEXT, 0);
            lv_obj_clear_flag(pr.unit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(pr.value, LV_ALIGN_CENTER, 0, -6);
        } else {
            lv_label_set_text(pr.value, "+");
            lv_obj_set_style_text_color(pr.value, C_MUTED, 0);
            lv_obj_add_flag(pr.unit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(pr.value, LV_ALIGN_CENTER, 0, 0);
        }
        const bool match = pr.lb > 0 && pr.lb == ui.weight;
        lv_obj_set_style_border_color(pr.btn, match ? C_TEXT : C_TRACK, 0);
    }
}

/** Tap: send the stored weight to the Voltra. */
void on_preset_tap(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    const int lb = ui.presets[i].lb;
    if (lb <= 0) {
        haptics_buzz();   // empty: hold to store one first
        return;
    }
    haptics_click();
    set_weight(lb);
    refresh_main();
}

/** Press and hold: store the current weight. */
void on_preset_hold(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    ui.presets[i].lb = ui.weight;
    save_preset(i);
    haptics_double();
    refresh_presets();
}

void make_preset(int i, int x, int y)
{
    auto &pr = ui.presets[i];
    pr.btn = make_pill_button(ui.scr_main, 50, 50, C_TRACK, "", nullptr);
    lv_obj_align(pr.btn, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_bg_color(pr.btn, C_MUTED, LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(pr.btn, 3);
    // Short-clicked, not clicked: LVGL still sends CLICKED on release after a long
    // press, which would apply the preset that was just stored.
    lv_obj_add_event_cb(pr.btn, on_preset_tap, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(pr.btn, on_preset_hold, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    pr.value = make_label(pr.btn, &font_poppins_18, C_TEXT, "+");
    lv_obj_align(pr.value, LV_ALIGN_CENTER, 0, -6);
    pr.unit = make_label(pr.btn, &font_poppins_14, C_MUTED, "lb");
    lv_obj_align(pr.unit, LV_ALIGN_CENTER, 0, 12);
}

void refresh_main()
{
    const DeviceState &st = ui.st;
    bool connected = st.connected();
    bool ready = st.conn == ConnState::Ready;
    uint32_t now = millis();

    // track + weight. Until the Voltra has reported its weight there is no weight to show,
    // so the link state takes its place.
    const bool weight_known = ready && st.weight >= voltra::MIN_TARGET_LB;
    lv_label_set_text_fmt(ui.lbl_weight, "%d", ui.weight);
    set_obj_hidden(ui.row_weight, !weight_known);
    set_obj_hidden(ui.lbl_link, weight_known);
    if (!weight_known) lv_label_set_text(ui.lbl_link, connected ? "Connecting..." : "Not\nConnected");

    bool loaded = st.loaded();
    bool pending = now < ui.toggle_pending_until && loaded != ui.toggle_target_loaded;
    lv_color_t ring = loaded ? C_LOADED : C_IDLE_RING;
    if (!connected) ring = C_TRACK;
    track_set(ui.track_weight, voltra::MIN_TARGET_LB, weight_max(),
              weight_known ? ui.weight : voltra::MIN_TARGET_LB, ring);

    // A press-to-load runs briefly through the auto-load states; show it as loading rather
    // than asking for a pull.
    const bool tap_loading = now < ui.toggle_pending_until && ui.toggle_target_loaded;
    const bool auto_loading = connected && !tap_loading &&
                              (st.auto_loading() || now < ui.auto_load_pending_until);
    if (!weight_known) {
        lv_label_set_text(ui.lbl_state, "");   // the big link message says it all
    } else if (auto_loading && !loaded) {
        // The Voltra's auto load: waiting for the cable to be pulled out and held, then
        // a 3 s countdown before it loads.
        const int ms = st.direct_load_countdown_ms;
        if (ms > 0 && ms <= 3000) lv_label_set_text_fmt(ui.lbl_state, "LOADING IN %d", (ms + 999) / 1000);
        else lv_label_set_text(ui.lbl_state, "PULL & HOLD CABLE");
        lv_obj_set_style_text_color(ui.lbl_state, C_WARN, 0);
        lv_obj_set_style_text_color(ui.lbl_weight, C_IDLE_NUM, 0);
    } else if (pending) {
        lv_label_set_text(ui.lbl_state, ui.toggle_target_loaded ? "LOADING..." : "UNLOADING...");
        lv_obj_set_style_text_color(ui.lbl_state, C_WARN, 0);
        lv_obj_set_style_text_color(ui.lbl_weight, loaded ? C_LOADED : C_IDLE_NUM, 0);
    } else if (loaded) {
        lv_label_set_text(ui.lbl_state, "Press to Unload");
        lv_obj_set_style_text_color(ui.lbl_state, C_TEXT, 0);
        lv_obj_set_style_text_color(ui.lbl_weight, C_LOADED, 0);
    } else if (st.activation == 0) {
        lv_label_set_text(ui.lbl_state, "NOT ACTIVATED");
        lv_obj_set_style_text_color(ui.lbl_state, C_DANGER, 0);
        lv_obj_set_style_text_color(ui.lbl_weight, C_IDLE_NUM, 0);
    } else {
        lv_label_set_text(ui.lbl_state, "Press to Load");
        lv_obj_set_style_text_color(ui.lbl_state, C_TEXT, 0);
        lv_obj_set_style_text_color(ui.lbl_weight, C_IDLE_NUM, 0);
    }

    // top bar: Bluetooth icon (blue when connected, white when not) and, once connected,
    // the Voltra's battery level. Recolour markup colours just the icon.
    char top[64];
    const char *bt_hex = connected ? C_BT_HEX : C_WHITE_HEX;
    if (connected && st.battery >= 0) {
        const char *sym = st.battery > 80 ? LV_SYMBOL_BATTERY_FULL
                        : st.battery > 60 ? LV_SYMBOL_BATTERY_3
                        : st.battery > 40 ? LV_SYMBOL_BATTERY_2
                        : st.battery > 15 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
        snprintf(top, sizeof(top), "#%s " LV_SYMBOL_BLUETOOTH "#   %s %d%%", bt_hex, sym, st.battery);
    } else {
        snprintf(top, sizeof(top), "#%s " LV_SYMBOL_BLUETOOTH "#", bt_hex);
    }
    lv_label_set_text(ui.lbl_top, top);
    lv_obj_set_style_text_color(ui.lbl_top, C_TEXT, 0);

    // During a set the controls under the track give way to big set / rep counters.
    const bool set_mode = ready && loaded && ui.set_active;
    lv_obj_t *const set_widgets[] = {ui.lbl_set_cap, ui.lbl_set_num, ui.lbl_rep_cap};
    for (lv_obj_t *o : set_widgets) {
        if (set_mode) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    // No rep counted yet: pulsing dots in place of a 0.
    const bool waiting_for_rep = set_mode && st.reps == 0;
    set_obj_hidden(ui.lbl_rep_num, !set_mode || waiting_for_rep);
    set_rep_dots(waiting_for_rep);
    if (set_mode) {
        lv_label_set_text_fmt(ui.lbl_set_num, "%u", (unsigned)st.sets);
        lv_label_set_text_fmt(ui.lbl_rep_num, "%u", (unsigned)st.reps);
    }

    // With an accessory on, the resistance changes through the rep. During a set the big
    // number follows it, as on the Voltra, while the cable is moving (between reps the
    // cable goes slack and the reading falls towards 0) and unless the dial is being
    // turned, when it shows the base being set.
    const bool accessory_on = ui.chains > 0 || ui.ecc != 0;
    const bool moving = st.rep_phase >= 1 && st.rep_phase <= 3;
    if (set_mode && accessory_on && moving && st.live_force_tenths >= 0 && now >= ui.editing_until) {
        lv_label_set_text_fmt(ui.lbl_weight, "%d", (st.live_force_tenths + 5) / 10);
        lv_label_set_text_fmt(ui.lbl_state, "Live  (base %d lb)", ui.weight);
        lv_obj_set_style_text_color(ui.lbl_state, C_TEXT, 0);
    }

    // reps / live force (small line, outside a set)
    if (set_mode) {
        lv_label_set_text(ui.lbl_reps, "");
    } else if (ready && loaded) {
        if (st.reps > 0 || st.sets > 0) {
            lv_label_set_text_fmt(ui.lbl_reps, "SET %u   REP %u", (unsigned)st.sets, (unsigned)st.reps);
        } else if (st.force_known) {
            lv_label_set_text_fmt(ui.lbl_reps, "%d lb on cable", st.force_lb);
        } else {
            lv_label_set_text(ui.lbl_reps, "");
        }
    } else {
        lv_label_set_text(ui.lbl_reps, "");
    }

    // settings button: white with nothing on, green once any accessory is on
    const bool any_accessory = ui.chains > 0 || ui.ecc != 0;
    lv_obj_set_style_text_color(ui.lbl_gear, any_accessory ? C_LOADED : C_TEXT, 0);

    // eccentric chip (left)
    if (ui.ecc != 0) {
        set_chip_values(ui.chip_ecc, ui.ecc, true);
        lv_obj_clear_flag(ui.chip_ecc.btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.chip_ecc.btn, LV_OBJ_FLAG_HIDDEN);
    }

    // chains / inverse chains / mountain chip (right), whichever style is live
    if (ui.chains > 0) {
        const char *icon = ui.style == ChainStyle::Inverse ? ICON_INVERSE
                         : ui.style == ChainStyle::Mountain ? ICON_MOUNTAIN : ICON_CHAINS;
        const lv_color_t col = style_colour(ui.style);
        lv_label_set_text(ui.chip_chain.icon, icon);
        lv_obj_set_style_text_color(ui.chip_chain.icon, col, 0);
        lv_obj_set_style_border_color(ui.chip_chain.btn, col, 0);
        lv_obj_set_style_bg_color(ui.chip_chain.btn, col, LV_STATE_PRESSED);
        set_chip_values(ui.chip_chain, ui.chains, false);
        lv_obj_clear_flag(ui.chip_chain.btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.chip_chain.btn, LV_OBJ_FLAG_HIDDEN);
    }

    refresh_presets();

    if (set_mode) {
        lv_obj_add_flag(ui.btn_gear, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.chip_ecc.btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.chip_chain.btn, LV_OBJ_FLAG_HIDDEN);
        for (auto &pr : ui.presets) lv_obj_add_flag(pr.btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui.btn_gear, LV_OBJ_FLAG_HIDDEN);
        for (auto &pr : ui.presets) lv_obj_clear_flag(pr.btn, LV_OBJ_FLAG_HIDDEN);
        // the chips were shown or hidden above according to the accessories
    }
}

void on_top_clicked(lv_event_t *)
{
    haptics_click();
    show(Screen::Connect);
    VClient::instance().scanStart();
}

/**
 * Push button: load / unload, from any screen. Leaving another screen comes back to the
 * weight so the result is on show. With no Voltra connected it opens the connect screen.
 */
void on_button_click()
{
    VClient &c = VClient::instance();
    if (!ui.st.connected()) {
        if (ui.screen == Screen::Connect) {
            haptics_buzz();   // already looking for one
            return;
        }
        haptics_click();
        show(Screen::Connect);
        c.scanStart();
        return;
    }
    if (ui.st.conn != ConnState::Ready) {
        haptics_buzz();
        return;
    }
    if (ui.screen != Screen::Main) {
        if (ui.screen == Screen::Connect) c.scanStop();
        show(Screen::Main);
    }
    // Push any pending dial edits first so the device loads the weight on screen.
    flush_pending(millis() + SEND_DEBOUNCE_MS);
    if (!ui.st.loaded() && (ui.st.auto_loading() || millis() < ui.auto_load_pending_until)) {
        // Pressing during auto load cancels it.
        ui.auto_load_pending_until = 0;
        c.unload();
        haptics_click();
        refresh_main();
        return;
    }
    bool loaded = ui.st.loaded();
    ui.toggle_target_loaded = !loaded;
    ui.toggle_pending_until = millis() + TOGGLE_PENDING_MS;
    // load() picks a plain load or, with the cable pulled out, a direct load.
    if (loaded) c.unload(); else c.load();
    haptics_double();
    refresh_main();
}

/**
 * Hold the button about a second and let go: start the Voltra's auto load (loads once the
 * cable is pulled and held). Holding on past that runs the power-off countdown instead.
 */
void on_button_hold()
{
    if (ui.st.conn != ConnState::Ready || ui.st.loaded() || ui.st.auto_loading()) {
        haptics_buzz();
        return;
    }
    if (ui.screen != Screen::Main) {
        if (ui.screen == Screen::Connect) VClient::instance().scanStop();
        show(Screen::Main);
    }
    flush_pending(millis() + SEND_DEBOUNCE_MS);
    ui.toggle_pending_until = 0;
    ui.auto_load_pending_until = millis() + AUTO_LOAD_PENDING_MS;
    VClient::instance().autoLoad();
    haptics_double();
    refresh_main();
}

/**
 * Tapping the weight while disconnected opens the connect screen, as the top bar does.
 * Loading is on the push button only, so a stray touch on the screen never loads the cable.
 */
void on_center_clicked(lv_event_t *e)
{
    if (!ui.st.connected()) on_top_clicked(e);
}

void on_gear_clicked(lv_event_t *);
void refresh_settings();
void refresh_adjust();

/** Tapping a chip goes straight to that accessory's dial, and Done comes back here. */
void on_chip_clicked(lv_event_t *e)
{
    haptics_click();
    ui.adj_return = Screen::Main;
    if (lv_event_get_target(e) == ui.chip_ecc.btn) {
        show(Screen::AdjustEcc);
    } else {
        ui.adj_style = ui.style;
        show(Screen::AdjustChains);
    }
    refresh_adjust();
}

/** A small pill: an active accessory's icon and its amount. */
void make_chip(Ui::Chip &c, const char *icon, lv_color_t colour, int x, int y)
{
    c.btn = make_pill_button(ui.scr_main, 80, 30, colour, "", nullptr);
    lv_obj_align(c.btn, LV_ALIGN_CENTER, x, y);
    lv_obj_set_ext_click_area(c.btn, 4);
    lv_obj_add_event_cb(c.btn, on_chip_clicked, LV_EVENT_CLICKED, nullptr);
    c.icon = make_label(c.btn, &font_icons_18, colour, icon);
    lv_obj_align(c.icon, LV_ALIGN_LEFT_MID, 9, 0);
    c.primary = make_label(c.btn, &font_poppins_14, C_TEXT, "");
    lv_obj_align(c.primary, LV_ALIGN_CENTER, 11, 1);
    lv_obj_add_flag(c.btn, LV_OBJ_FLAG_HIDDEN);
}

void build_main()
{
    ui.scr_main = make_screen();
    ui.track_weight = make_track(ui.scr_main, ui.track_weight_st, C_IDLE_RING);

    // top bar, left: Bluetooth and the Voltra's battery (tap -> connect menu)
    ui.btn_top = make_flat_button(ui.scr_main, 120, 28);
    lv_obj_set_style_radius(ui.btn_top, 8, 0);
    lv_obj_align(ui.btn_top, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(ui.btn_top, on_top_clicked, LV_EVENT_CLICKED, nullptr);
    ui.lbl_top = make_label(ui.btn_top, &font_poppins_16, C_TEXT, "");
    lv_label_set_recolor(ui.lbl_top, true);
    lv_label_set_text(ui.lbl_top, "#" C_WHITE_HEX " " LV_SYMBOL_BLUETOOTH "#");
    lv_obj_align(ui.lbl_top, LV_ALIGN_LEFT_MID, 4, 0);

    // top bar, right: this remote's own battery
    ui.lbl_kbat = make_label(ui.scr_main, &font_poppins_14, C_MUTED, "");
    lv_obj_align(ui.lbl_kbat, LV_ALIGN_TOP_RIGHT, -6, 5);

    // inside the track: the weight, or the link state until there is one
    ui.btn_center = make_flat_button(ui.scr_main, TRACK_INNER_W, TRACK_INNER_H);
    lv_obj_align(ui.btn_center, LV_ALIGN_CENTER, 0, TRACK_DY);
    lv_obj_add_event_cb(ui.btn_center, on_center_clicked, LV_EVENT_CLICKED, nullptr);
    ui.row_weight = make_value_stack(ui.btn_center, &font_poppins_bold_56, &ui.lbl_weight, &ui.lbl_unit);
    // The digits sit low in the font's line box; lift the stack so they look centred.
    lv_obj_align(ui.row_weight, LV_ALIGN_CENTER, 0, -9);
    ui.lbl_link = make_label(ui.btn_center, &font_poppins_20, C_MUTED, "Not\nConnected");
    // Wraps rather than running past the track (20 px fits "Connecting..." on one line).
    lv_obj_set_width(ui.lbl_link, TRACK_INNER_W);
    lv_obj_set_style_text_align(ui.lbl_link, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.lbl_link, LV_ALIGN_CENTER, 0, 0);

    // under the track: what the button will do, then reps / force on the cable
    ui.lbl_state = make_label(ui.scr_main, &font_poppins_14, C_MUTED, "");
    lv_obj_align(ui.lbl_state, LV_ALIGN_CENTER, 0, ROW_STATE_DY);
    ui.lbl_reps = make_label(ui.scr_main, &font_poppins_14, C_MUTED, "");
    lv_obj_align(ui.lbl_reps, LV_ALIGN_CENTER, 0, ROW_INFO_DY);

    // During a set: two big counters filling the space the controls use at rest.
    constexpr int COL_X = SCR_W / 4;
    ui.lbl_set_cap = make_label(ui.scr_main, &font_poppins_16, C_MUTED, "Set");
    ui.lbl_rep_cap = make_label(ui.scr_main, &font_poppins_16, C_MUTED, "Reps");
    ui.lbl_set_num = make_label(ui.scr_main, &font_poppins_bold_56, C_TEXT, "0");
    ui.lbl_rep_num = make_label(ui.scr_main, &font_poppins_bold_56, C_TEXT, "0");
    lv_obj_t *const caps[] = {ui.lbl_set_cap, ui.lbl_rep_cap};
    lv_obj_align(ui.lbl_set_cap, LV_ALIGN_CENTER, -COL_X, 56);
    lv_obj_align(ui.lbl_rep_cap, LV_ALIGN_CENTER, COL_X, 56);
    lv_obj_t *const nums[] = {ui.lbl_set_num, ui.lbl_rep_num};
    for (lv_obj_t *o : nums) {
        // centre the digits on their column however many there are
        lv_obj_set_width(o, SCR_W / 2);
        lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    }
    // digits run from ~79 to ~120 px below centre, under the captions
    lv_obj_align(ui.lbl_set_num, LV_ALIGN_CENTER, -COL_X, 102);
    lv_obj_align(ui.lbl_rep_num, LV_ALIGN_CENTER, COL_X, 102);
    for (lv_obj_t *o : caps) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t *o : nums) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);

    // Waiting-for-first-rep dots, sitting on the digits' baseline in the reps column.
    ui.rep_dots = lv_obj_create(ui.scr_main);
    lv_obj_remove_style_all(ui.rep_dots);
    lv_obj_set_size(ui.rep_dots, 56, 12);
    lv_obj_clear_flag(ui.rep_dots, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ui.rep_dots, LV_ALIGN_CENTER, COL_X, 113);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *d = lv_obj_create(ui.rep_dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 12, 12);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, C_LOADED, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_30, 0);
        lv_obj_align(d, LV_ALIGN_LEFT_MID, i * 22, 0);
        ui.rep_dot[i] = d;
    }
    lv_obj_add_flag(ui.rep_dots, LV_OBJ_FLAG_HIDDEN);

    // Chips for the active accessories, shown only while on.
    make_chip(ui.chip_ecc, ICON_ECCENTRIC, C_ECC, -42, ROW_CHIPS_DY);
    make_chip(ui.chip_chain, ICON_CHAINS, C_CHAINS, 42, ROW_CHIPS_DY);

    // Bottom row: a weight preset either side of the settings button.
    ui.btn_gear = make_pill_button(ui.scr_main, 50, 50, C_TRACK, "", nullptr);
    lv_obj_align(ui.btn_gear, LV_ALIGN_CENTER, 0, ROW_BOTTOM_DY);
    lv_obj_set_ext_click_area(ui.btn_gear, 3);
    lv_obj_add_event_cb(ui.btn_gear, on_gear_clicked, LV_EVENT_CLICKED, nullptr);
    ui.lbl_gear = make_label(ui.btn_gear, &font_icons_26, C_TEXT, ICON_SETTINGS);
    lv_obj_center(ui.lbl_gear);

    load_presets();
    make_preset(0, -58, ROW_BOTTOM_DY);
    make_preset(1, 58, ROW_BOTTOM_DY);
    refresh_presets();
}

// ---------------------------------------------------------------------------
// Adjust screen (chains / eccentric)
// ---------------------------------------------------------------------------
void refresh_adjust()
{
    bool chains = ui.screen == Screen::AdjustChains;
    lv_label_set_text(ui.lbl_adj_unit, accessory_unit());
    if (chains) {
        int mx = chains_max();
        const int v = style_amount(ui.adj_style);
        const lv_color_t col = style_colour(ui.adj_style);
        lv_label_set_text(ui.lbl_adj_title, style_name(ui.adj_style));
        lv_obj_set_style_text_color(ui.lbl_adj_title, col, 0);
        track_set(ui.track_adj, 0, std::max(mx, STEP_COARSE_LB), v, col);
        lv_label_set_text_fmt(ui.lbl_adj_val, "%d", v);
        if (lb_mode()) {
            lv_label_set_text_fmt(ui.lbl_adj_range, "0 - %d lb\n= %d%% of %d lb",
                                  mx, amount_pct(v), ui.weight);
        } else {
            lv_label_set_text_fmt(ui.lbl_adj_range, "0 - %d%%\n= %d lb of %d lb",
                                  mx, pct_to_lb(v), ui.weight);
        }
    } else {
        int mx = ecc_max();
        lv_label_set_text(ui.lbl_adj_title, "Eccentric");
        lv_obj_set_style_text_color(ui.lbl_adj_title, C_ECC, 0);
        const int span = std::max(mx, STEP_COARSE_LB);
        track_set(ui.track_adj, -span, span, ui.ecc, C_ECC, true);
        lv_label_set_text_fmt(ui.lbl_adj_val, "%+d", ui.ecc);
        if (lb_mode()) {
            lv_label_set_text_fmt(ui.lbl_adj_range, "-%d - +%d lb\n= %+d%% of %d lb",
                                  mx, mx, amount_pct(ui.ecc), ui.weight);
        } else {
            lv_label_set_text_fmt(ui.lbl_adj_range, "-%d - +%d%%\n= %+d lb of %d lb",
                                  mx, mx, pct_to_lb(ui.ecc), ui.weight);
        }
    }
}

void refresh_settings();

void on_adjust_done(lv_event_t *)
{
    haptics_click();
    flush_pending(millis() + SEND_DEBOUNCE_MS);
    show(ui.adj_return);
    if (ui.adj_return == Screen::Main) refresh_main();
    else refresh_settings();
}

void on_gear_clicked(lv_event_t *)
{
    haptics_click();
    show(Screen::Settings);
    refresh_settings();
}

void build_adjust()
{
    ui.scr_adjust = make_screen();
    ui.track_adj = make_track(ui.scr_adjust, ui.track_adj_st, C_CHAINS);

    ui.lbl_adj_title = make_title(ui.scr_adjust, "Chains", C_CHAINS);
    lv_obj_set_style_text_letter_space(ui.lbl_adj_title, 0, 0);
    lv_obj_align(ui.lbl_adj_title, LV_ALIGN_TOP_MID, 0, 0);   // just clears the track

    ui.btn_adj_center = make_flat_button(ui.scr_adjust, TRACK_INNER_W, TRACK_INNER_H);
    lv_obj_align(ui.btn_adj_center, LV_ALIGN_CENTER, 0, TRACK_DY);
    lv_obj_add_event_cb(ui.btn_adj_center, on_adjust_done, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *stack = make_value_stack(ui.btn_adj_center, &font_poppins_bold_56, &ui.lbl_adj_val, &ui.lbl_adj_unit);
    lv_obj_align(stack, LV_ALIGN_CENTER, 0, -9);

    // range and the other unit, two lines under the track
    ui.lbl_adj_range = make_label(ui.scr_adjust, &font_poppins_14, C_MUTED, "");
    lv_obj_set_width(ui.lbl_adj_range, SCR_W - 8);
    lv_obj_set_style_text_align(ui.lbl_adj_range, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.lbl_adj_range, LV_ALIGN_CENTER, 0, ROW_STATE_DY + 10);

    ui.btn_done = make_action_button(ui.scr_adjust, "DONE");
    lv_obj_add_event_cb(ui.btn_done, on_adjust_done, LV_EVENT_CLICKED, nullptr);
}


// ---------------------------------------------------------------------------
// Settings menu (chains, inverse chains, eccentric, mountain)
// ---------------------------------------------------------------------------
void on_settings_row(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const intptr_t row = (intptr_t)lv_obj_get_user_data(btn);
    haptics_click();
    switch (row) {
        case ROW_CHAINS:
        case ROW_INVERSE:
        case ROW_MOUNTAIN:
            ui.adj_style = row == ROW_INVERSE ? ChainStyle::Inverse
                         : row == ROW_MOUNTAIN ? ChainStyle::Mountain : ChainStyle::Chains;
            ui.adj_return = Screen::Settings;
            show(Screen::AdjustChains);
            refresh_adjust();
            break;
        case ROW_ECCENTRIC:
            ui.adj_return = Screen::Settings;
            show(Screen::AdjustEcc);
            refresh_adjust();
            break;
        case ROW_CLOSE:
        default:
            show(Screen::Main);
            refresh_main();
            break;
    }
}

/** One accessory tile: the accessory's icon, a small title, and its current value. */
void make_bubble(Ui::Bubble &b, const char *icon, const char *title, intptr_t row, int x, int y)
{
    b.btn = make_pill_button(ui.scr_settings, 78, 100, C_TRACK, "", nullptr);
    lv_obj_set_style_radius(b.btn, 16, 0);
    lv_obj_align(b.btn, LV_ALIGN_CENTER, x, y);
    lv_obj_set_user_data(b.btn, (void *)row);
    lv_obj_add_event_cb(b.btn, on_settings_row, LV_EVENT_CLICKED, nullptr);
    b.icon = make_label(b.btn, &font_icons_26, C_TEXT, icon);
    lv_obj_align(b.icon, LV_ALIGN_CENTER, 0, -26);
    b.title = make_label(b.btn, &font_poppins_14, C_MUTED, title);
    lv_obj_align(b.title, LV_ALIGN_CENTER, 0, 1);
    b.value = make_label(b.btn, &font_poppins_20, C_TEXT, "--");
    lv_obj_align(b.value, LV_ALIGN_CENTER, 0, 26);
}

void set_bubble(Ui::Bubble &b, const char *value, bool active, lv_color_t colour)
{
    lv_label_set_text(b.value, value);
    lv_obj_set_style_border_color(b.btn, active ? colour : C_TRACK, 0);
    lv_obj_set_style_bg_color(b.btn, active ? colour : C_CARD, 0);
    lv_obj_set_style_bg_opa(b.btn, active ? LV_OPA_20 : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(b.icon, active ? colour : C_TEXT, 0);
    lv_obj_set_style_text_color(b.title, active ? colour : C_MUTED, 0);
}

void refresh_settings()
{
    char buf[16];

    if (ui.ecc != 0) format_amount(buf, sizeof(buf), ui.ecc, true);
    else snprintf(buf, sizeof(buf), "OFF");
    set_bubble(ui.bub_ecc, buf, ui.ecc != 0, C_ECC);

    // one amount, three styles: only the live style's bubble shows it
    const struct { Ui::Bubble &b; ChainStyle s; } chain_bubbles[] = {
        {ui.bub_chains, ChainStyle::Chains},
        {ui.bub_inverse, ChainStyle::Inverse},
        {ui.bub_mountain, ChainStyle::Mountain},
    };
    for (const auto &cb : chain_bubbles) {
        const int v = style_amount(cb.s);
        if (v > 0) format_amount(buf, sizeof(buf), v, false);
        else snprintf(buf, sizeof(buf), "OFF");
        set_bubble(cb.b, buf, v > 0, style_colour(cb.s));
    }
}

void build_settings()
{
    ui.scr_settings = make_screen();
    make_title(ui.scr_settings, "SETTINGS", C_TEXT);

    // 2x2 grid between the title and Done, in the order eccentric, chains / mountain,
    // inverse chains.
    make_bubble(ui.bub_ecc,      ICON_ECCENTRIC, "Eccentric", ROW_ECCENTRIC, -41, -70);
    make_bubble(ui.bub_chains,   ICON_CHAINS,    "Chains",    ROW_CHAINS,     41, -70);
    make_bubble(ui.bub_mountain, ICON_MOUNTAIN,  "Mountain",  ROW_MOUNTAIN,  -41,  36);
    make_bubble(ui.bub_inverse,  ICON_INVERSE,   "Inverse",   ROW_INVERSE,    41,  36);

    // Done along the bottom.
    ui.btn_settings_done = make_action_button(ui.scr_settings, "DONE");
    lv_obj_set_user_data(ui.btn_settings_done, (void *)(intptr_t)ROW_CLOSE);
    lv_obj_add_event_cb(ui.btn_settings_done, on_settings_row, LV_EVENT_CLICKED, nullptr);
}

// ---------------------------------------------------------------------------
// Connect screen
// ---------------------------------------------------------------------------
void on_device_clicked(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    intptr_t idx = (intptr_t)lv_obj_get_user_data(btn);
    haptics_click();
    VClient &c = VClient::instance();
    if (idx < 0) {
        c.disconnect();
        c.scanStart();
        return;
    }
    if ((size_t)idx < ui.devs.size()) {
        c.scanStop();
        c.connectTo(ui.devs[idx]);
        show(Screen::Main);
        refresh_main();
    }
}

void on_close_clicked(lv_event_t *)
{
    haptics_click();
    VClient::instance().scanStop();
    show(Screen::Main);
    refresh_main();
}

void rebuild_device_list()
{
    lv_obj_clean(ui.list);
    const DeviceState &st = ui.st;
    if (st.connected() || st.conn == ConnState::Connecting || st.conn == ConnState::Reconnecting) {
        const char *name = st.device_name[0] ? st.device_name : "Voltra";
        char buf[48];
        snprintf(buf, sizeof(buf), "Disconnect %s", name);
        lv_obj_t *b = lv_list_add_btn(ui.list, LV_SYMBOL_CLOSE, buf);
        lv_obj_set_user_data(b, (void *)(intptr_t)-1);
        lv_obj_set_style_text_color(b, C_DANGER, 0);
        lv_obj_add_event_cb(b, on_device_clicked, LV_EVENT_CLICKED, nullptr);
    }
    for (size_t i = 0; i < ui.devs.size(); i++) {
        const FoundDevice &d = ui.devs[i];
        if (st.connected() && strcmp(d.address.c_str(), st.address) == 0) continue;
        char buf[48];
        snprintf(buf, sizeof(buf), "%s  %d", d.name.c_str(), d.rssi);
        lv_obj_t *b = lv_list_add_btn(ui.list, LV_SYMBOL_BLUETOOTH, buf);
        lv_obj_set_user_data(b, (void *)(intptr_t)i);
        lv_obj_set_style_text_color(b, C_TEXT, 0);
        lv_obj_add_event_cb(b, on_device_clicked, LV_EVENT_CLICKED, nullptr);
    }
    if (ui.devs.empty() && !st.connected()) {
        lv_obj_t *t = lv_list_add_text(ui.list, "No Voltra found yet.\nSwitch the Voltra on.");
        lv_obj_set_style_text_color(t, C_MUTED, 0);
    }
}

void refresh_connect()
{
    const DeviceState &st = ui.st;
    if (st.conn == ConnState::Scanning) {
        lv_label_set_text(ui.lbl_conn_status, "Scanning for Voltras");
    } else {
        lv_label_set_text(ui.lbl_conn_status, st.status);
    }
    // The scan runs in repeating windows while this screen is open.
    lv_obj_clear_flag(ui.spinner, LV_OBJ_FLAG_HIDDEN);
}

void build_connect()
{
    ui.scr_connect = make_screen();
    make_title(ui.scr_connect, "CONNECT", C_TEXT);

    ui.lbl_conn_status = make_label(ui.scr_connect, &font_poppins_14, C_MUTED, "");
    lv_obj_set_width(ui.lbl_conn_status, SCR_W - 8);
    lv_label_set_long_mode(ui.lbl_conn_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui.lbl_conn_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.lbl_conn_status, LV_ALIGN_TOP_MID, 0, 34);

    ui.spinner = lv_spinner_create(ui.scr_connect, 1200, 60);
    lv_obj_set_size(ui.spinner, 20, 20);
    lv_obj_set_style_arc_width(ui.spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.spinner, C_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.spinner, C_UNLOADED, LV_PART_INDICATOR);
    lv_obj_align(ui.spinner, LV_ALIGN_TOP_MID, 0, 58);

    ui.list = lv_list_create(ui.scr_connect);
    lv_obj_set_size(ui.list, SCR_W - 8, 172);
    lv_obj_align(ui.list, LV_ALIGN_TOP_MID, 0, 84);
    lv_obj_set_style_bg_color(ui.list, C_CARD, 0);
    lv_obj_set_style_border_width(ui.list, 0, 0);
    lv_obj_set_style_radius(ui.list, 14, 0);
    lv_obj_set_style_pad_all(ui.list, 4, 0);
    lv_obj_set_style_text_font(ui.list, &font_poppins_14, 0);
    lv_obj_set_scrollbar_mode(ui.list, LV_SCROLLBAR_MODE_OFF);

    ui.btn_close = make_action_button(ui.scr_connect, "CLOSE");
    lv_obj_add_event_cb(ui.btn_close, on_close_clicked, LV_EVENT_CLICKED, nullptr);
}

// ---------------------------------------------------------------------------
// Power off: hold the button through a countdown
// ---------------------------------------------------------------------------
void build_power_overlay()
{
    ui.power_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ui.power_overlay);
    lv_obj_set_size(ui.power_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ui.power_overlay, C_BG, 0);
    lv_obj_set_style_bg_opa(ui.power_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(ui.power_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    ui.lbl_power_title = make_label(ui.power_overlay, &font_poppins_20, C_TEXT, "Power off");
    lv_obj_align(ui.lbl_power_title, LV_ALIGN_CENTER, 0, -64);
    ui.lbl_power_count = make_label(ui.power_overlay, &font_poppins_bold_56, C_WARN, "");
    lv_obj_align(ui.lbl_power_count, LV_ALIGN_CENTER, 0, 0);
    ui.lbl_power_hint = make_label(ui.power_overlay, &font_poppins_14, C_MUTED, "Let go to cancel");
    lv_obj_align(ui.lbl_power_hint, LV_ALIGN_CENTER, 0, 60);
    lv_obj_add_flag(ui.power_overlay, LV_OBJ_FLAG_HIDDEN);
}

/** Show the countdown once a hold has gone past auto load, seconds left to power off. */
void refresh_power_overlay()
{
    const uint32_t held = button_held_ms();
    const bool counting = held >= BUTTON_CANCEL_MS;
    set_obj_hidden(ui.power_overlay, !counting);
    if (counting) {
        const uint32_t left = held < POWER_OFF_HOLD_MS ? POWER_OFF_HOLD_MS - held : 0;
        lv_label_set_text_fmt(ui.lbl_power_count, "%u", (unsigned)((left + 999) / 1000));
    }
}

/** Held through the countdown: let the Voltra go, then power off. Does not return. */
void on_power_off()
{
    lv_label_set_text(ui.lbl_power_title, "Powering off");
    lv_obj_add_flag(ui.lbl_power_count, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui.lbl_power_hint, "Let go of the button");
    set_obj_hidden(ui.power_overlay, false);
    lv_refr_now(nullptr);

    // Disconnect cleanly so the Voltra sees the remote go at once rather than after a
    // link timeout. The saved Voltra is kept, so it reconnects after powering on.
    VClient &c = VClient::instance();
    if (c.state().conn != ConnState::Idle) {
        c.disconnect();
        for (int i = 0; i < 100 && c.state().conn != ConnState::Idle; i++) delay(10);
    }
    power_off();
}

// ---------------------------------------------------------------------------
// Periodic poll: knob, device state, debounced sends
// ---------------------------------------------------------------------------
void handle_knob(int delta)
{
    const int step = ui.knob_rate.step(delta, millis());

    switch (ui.screen) {
        case Screen::Main: {
            if (ui.st.conn != ConnState::Ready || ui.st.weight < voltra::MIN_TARGET_LB) break;
            if (set_weight(apply_step(ui.weight, delta, step))) {
                haptics_tick();
                refresh_main();
            }
            break;
        }
        case Screen::AdjustChains: {
            const int cur = style_amount(ui.adj_style);
            int v = clampi(apply_step(cur, delta, step), 0, chains_max());
            if (v != cur) {
                // Dialling an amount into another style switches the Voltra over to it,
                // which turns the previous style off.
                if (v > 0 && ui.style != ui.adj_style) {
                    ui.style = ui.adj_style;
                    ui.style_dirty = true;
                }
                ui.chains = v;
                mark_chains_changed();
                haptics_tick();
                refresh_adjust();
            }
            break;
        }
        case Screen::AdjustEcc: {
            int m = ecc_max();
            int v = clampi(apply_step(ui.ecc, delta, step), -m, m);
            if (v != ui.ecc) {
                ui.ecc = v;
                mark_ecc_changed();
                haptics_tick();
                refresh_adjust();
            }
            break;
        }
        case Screen::Settings:
            break;   // all four bubbles are visible at once; selection is by touch
        case Screen::Connect:
            lv_obj_scroll_by(ui.list, 0, -delta * 44, LV_ANIM_ON);
            break;
    }
}

void poll_cb(lv_timer_t *)
{
    VClient &c = VClient::instance();
    uint32_t now = millis();

    int delta = knob_take_delta();
    if (delta) handle_knob(delta);

    switch (button_take_event()) {
        case ButtonEvent::Click: on_button_click(); break;
        case ButtonEvent::Hold: on_button_hold(); break;
        case ButtonEvent::PowerOff: on_power_off(); break;
        case ButtonEvent::None: break;
    }
    refresh_power_overlay();

    flush_pending(now);

    bool state_changed = c.version() != ui.last_state_version;
    bool devs_changed = c.devicesVersion() != ui.last_dev_version;

    if (state_changed) {
        ui.last_state_version = c.version();
        ui.st = c.state();
        if (ui.st.accessory_display != ui.last_display) {
            ui.last_display = ui.st.accessory_display;
            ui.c_dirty = ui.e_dirty = ui.style_dirty = false;
            ui.editing_until = 0;
        }
        // Adopt device values unless the user is mid-edit on the dial.
        if (now >= ui.editing_until) {
            if (ui.st.weight >= voltra::MIN_TARGET_LB && !ui.w_dirty) ui.weight = ui.st.weight;
            if (lb_mode()) {
                if (!ui.c_dirty) ui.chains = ui.st.chains_lb;
                if (!ui.e_dirty) ui.ecc = ui.st.eccentric_lb;
            } else {
                if (ui.st.chains >= 0 && !ui.c_dirty) ui.chains = ui.st.chains;
                if (ui.st.eccentric_known && !ui.e_dirty) ui.ecc = ui.st.eccentric;
            }
            if (!ui.c_dirty) ui.style = device_style(ui.st);
        }
        if (now < ui.toggle_pending_until && ui.st.loaded() == ui.toggle_target_loaded) {
            ui.toggle_pending_until = 0;   // confirmed
        }
    }
    if (state_changed) {
        // A set is under way while the Voltra's workout status says so and the cable
        // has actually moved (the status turns "active" on loading, before any rep).
        // It ends the moment the status goes to resting or unloading. The fitness mode
        // cannot do this: it stays at 1 through every set after the first.
        const bool moving = ui.st.rep_phase >= 1 && ui.st.rep_phase <= 3;
        const int ws = ui.st.workout_status;
        if (!ui.st.loaded()) {
            ui.set_active = false;
        } else if (ws >= 0) {
            if (ws != voltra::WORKOUT_STATUS_ACTIVE) ui.set_active = false;
            else if (moving) ui.set_active = true;
        } else {
            // no status seen yet: fall back to the mode, which covers the first set
            const int mode = ui.st.fitness_mode >= 0 ? (ui.st.fitness_mode & 0xFF) : -1;
            if (mode == voltra::FITNESS_MODE_STRENGTH_IDLE) ui.set_active = false;
            else if (moving) ui.set_active = true;
        }
    }
    if (ui.auto_load_pending_until && (ui.st.auto_loading() || ui.st.loaded())) {
        ui.auto_load_pending_until = 0;   // the Voltra has taken over
    }
    if (ui.auto_load_pending_until && now >= ui.auto_load_pending_until) {
        ui.auto_load_pending_until = 0;
        if (ui.screen == Screen::Main) refresh_main();
    }
    bool pending_expired = ui.toggle_pending_until && now >= ui.toggle_pending_until;
    if (pending_expired) ui.toggle_pending_until = 0;

    if (devs_changed) {
        ui.last_dev_version = c.devicesVersion();
        ui.devs = c.devices();
    }

    switch (ui.screen) {
        case Screen::Main:
            if (state_changed || pending_expired) refresh_main();
            break;
        case Screen::Settings:
            if (state_changed) refresh_settings();
            break;
        case Screen::AdjustChains:
        case Screen::AdjustEcc:
            if (state_changed) refresh_adjust();
            break;
        case Screen::Connect:
            if (state_changed || devs_changed) {
                rebuild_device_list();
                refresh_connect();
            }
            break;
    }


    // the remote's own battery, every 10 s
    if (now - ui.last_kbat_ms > 10000) {
        ui.last_kbat_ms = now;
        int pct = battery_percent();
        if (pct >= 0) {
            const char *sym = pct > 80 ? LV_SYMBOL_BATTERY_FULL : pct > 60 ? LV_SYMBOL_BATTERY_3
                            : pct > 40 ? LV_SYMBOL_BATTERY_2 : pct > 15 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
            lv_label_set_text_fmt(ui.lbl_kbat, "%s %d%%", sym, pct);
        } else {
            lv_label_set_text(ui.lbl_kbat, LV_SYMBOL_USB);
        }
    }
}

}  // namespace

void ui_init()
{
    build_main();
    build_settings();
    build_adjust();
    build_connect();
    build_power_overlay();
    lv_scr_load(ui.scr_main);
    ui.st = VClient::instance().state();
    refresh_main();
    lv_timer_create(poll_cb, 25, nullptr);
}
