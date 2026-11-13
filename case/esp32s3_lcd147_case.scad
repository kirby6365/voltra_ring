// Case for Waveshare ESP32-S3-Touch-LCD-1.47 + 10x25x35 mm LiPo battery,
// with a scroll wheel (mouse wheel encoder) set into the far corner -- it sticks out of both
// the display face and the end face -- and a wide button on the end face below it.
// Units: mm.  Model coordinates: Z = 0 is the display face, Z grows toward the back.
// The ESP32 board is centred on X/Y; the USB-C end is at -Y, the controls end at +Y.
//
// Layout: one open cavity for the board + battery, plus a controls bay at the far end.
// The controls live on a piece of protoboard standing upright across the case; it slides
// into grooves in the bay's side walls from the open back. Wires run from the back of the
// protoboard over a low rib into the main cavity.
//
// Assembly:
//   1. Fit the ESP32 board from the front, display out: tilt it, push the USB-C end in
//      first so the connector enters its cutout, then swing the far end down.
//   2. Put the button into its hole from inside (a bit of tape outside holds it).
//   3. Build the protoboard (see protoboard_template), wire it to the ESP32, and slide it
//      into its grooves from the open back, components facing the far end.
//   4. Push the wheel in through the corner opening from outside (diagonally), then push
//      the shaft in through the side hole. Turn the shaft until its thin tip drops into the
//      encoder, then turn the wheel until the shaft's thick hex drops into the wheel's
//      snug hex, and push it home.
//   5. Connect and lay in the battery. Hold the lid tilted ~10-15 deg (USB end up), lower
//      its far end in about 1 mm short of the end wall, slide it toward the end so the two
//      hooks catch the groove, swing it down flat, then drive 4x M2 screws into the ESP32's
//      brass standoffs (type set by lid_screw; length is echoed on render -- M2x12 socket
//      head cap screws at the current depth).
//
// Render a part with:  openscad -D 'part="tub"' -o tub.stl esp32s3_lcd147_case.scad
// part = "tub" | "lid" | "wheel" | "shaft" | "button" | "fit_test" | "fit_tips" | "template" (2D, SVG)
//      | "assembly" | "exploded" | "print"

part = "assembly";

/* ---------------- board (from Waveshare drawing + STEP mockup) ---------------- */
board_w      = 24.55;   // X
board_l      = 44.50;   // Y
board_r      = 5.75;    // corner radius of the LCD frame
frame_h      = 6.60;    // display frame + PCB thickness (PCB back face)
board_h      = 10.60;   // to the end of the brass standoffs
hole_dx      = 17.00;   // standoff spacing across width
hole_dy      = 39.00;   // standoff spacing along length
standoff_r   = 1.60;
usb_w        = 8.40;    // USB-C receptacle (mockup)
usb_z0       = 6.60;
usb_z1       = 9.20;
usb_protrude = 0.75;    // receptacle sticks past the frame end

/* ---------------- battery ---------------- */
bat_w = 25; bat_l = 35; bat_h = 10;
bat_top = 19.4;      // measured in the test print: battery's back face, from the display face
bat_gap = 0.5;       // left between the battery and the lid

/* ---------------- case parameters ---------------- */
fit        = 0.25;   // clearance around the board frame (per side)
far_extra  = 0.25;   // extra clearance at the far (non-USB) end so the board can be tilted in
bat_fit    = 0.40;   // clearance beside the battery (per side)
wall       = 1.60;   // wall thickness behind the board (board-side wall ends up ~2 mm)
wall_end   = 2.00;   // end-wall thickness at the USB end
lid_t      = 2.00;
edge_ch    = 0.60;   // outer edge chamfer
rim_proud  = 0.00;   // raise the rim above the glass (e.g. 0.4 for drop protection)

ledge_w    = 1.00;   // how far the corner ledges reach under the PCB edge
ledge_t    = 1.60;
ledge_y    = 19.8;   // ledges only exist beyond |Y| > ledge_y (clear of buttons/battery)
ledge_x    = 6.0;    // ...and beyond |X| > ledge_x (clear of the USB connector)
ledge_sclear = 1.1;  // clearance around the standoffs (they swing toward the ends while tilting in)

usb_cut_w  = 11.6;   // USB-C cutout, centred on the receptacle
usb_cut_h  = 6.4;
usb_cut_r  = 2.4;

/* ---------------- controls bay ---------------- */
rib_t      = 1.5;    // low wall that stops the ESP32 frame; wires pass over it
wire_gap   = 3.0;    // minimum space behind the protoboard for solder joints and wires
pb_t       = 1.6;    // protoboard thickness
pb_fit     = 0.3;    // groove clearance on the board thickness
groove_d   = 1.0;    // groove depth into each side wall
bay_wall   = 2.0;    // side-wall thickness in the bay
front_t    = 1.6;    // display-face plate over the bay
front_skin = 0.8;    // plate left over the encoder (it sits in a pocket in the plate)
end_t      = 2.0;    // far end wall
far_r      = 2.0;    // outer corner radius at the far end (seen from above)
top_skin   = 0.4;    // wall left over the encoder where the rounded top edge passes it
top_enc_clear = 0.1; // ...plus this much space round the encoder body's corner
                     // (the top-edge radius at the far end is derived below: as round as the
                     //  encoder body allows; the encoder pocket is trimmed to follow it)

// mouse wheel encoder (MECCANIXITY "4.5 mm"); axis height + body sizes from the listing, check yours
enc_axis_h = 4.5;    // shaft-hole centre height above the protoboard
enc_t      = 4.2;    // body thickness along the shaft (measured)
enc_w      = 9.6;    // body width across the shaft (in the board plane)
enc_top    = 3.9;    // body extends this far past the axis, away from the board
key_af     = 1.75;   // encoder centre hole: hex, across flats (measured)
// Small printed hexes come out off-size (keys fat, holes tight), so the two printed fits
// are set from test prints -- print part="fit_test" to pick them for your printer.
// A hex key only locks in a hex hole while the hole's across-flats is smaller than the
// key's across-corners (1.155 x its across-flats); past that it spins.
key_print  = 1.80;   // shaft tip that goes into the encoder, hex across flats (confirmed by test print)
key_len    = 3.0;    // how far the tip goes into the encoder
drive_af   = 2.60;   // thicker hex section that drives the wheel (passes the side-wall hole)
drive_bore = 2.80;   // snug hex in the wheel that grips it (confirmed by test print)
drive_len  = 4.0;    // length of the snug hex, at the encoder end of the wheel
pass_d     = 3.4;    // rest of the wheel bore: round clearance, so the shaft slides through

// wheel diameter is derived below: the largest that fits above the switch at this tub depth
rib_pitch  = 2.8;    // grip groove spacing round the rim
groove_w   = 1.6;    // groove width
groove_dp  = 1.0;    // groove depth
shaft_d    = 3.0;    // round bearing section of the shaft (in the side wall)

// 6x6x5 mm through-hole tact switch
sw_body    = 6.0;
sw_body_h  = 3.5;
sw_h       = 5.0;    // to the top of the plunger
sw_plunger = 3.5;
// wide button bar on the end face
btn_w      = 13.0;
btn_hgt    = 5.0;
btn_proud  = 2.5;    // how far it stands out of the end face
btn_clear  = 0.35;   // clearance in its hole, per side
btn_ch     = 0.6;    // chamfer round the pressing face
btn_lip    = 1.5;    // retaining wings either side of the bar, inside the end wall
btn_lip_t  = 1.2;
btn_pin_w  = 3.0;    // plunger pin from the bar back to the switch (full bar height, prints flat)
btn_pin_trim = 0.1;  // test fit (switch pressed flat on the protoboard): pin is 2.0 mm long,
                     // 0.1 shorter than modelled (it's dropped if trimmed to under 0.3)

// lid hooks at the far end (catch a groove inside the end wall)
hook_w     = 3.6;    // along X
hook_t     = 1.0;
hook_h     = 3.0;
hook_barb  = 0.6;

// M2 hardware
lid_screw  = "socket";  // lid screws: "socket" (ISO 4762 cap screw, head flush in a counterbore)
                        // or "countersunk" (DIN 965 flat head)
m2_clear   = 2.40;   // clearance hole in the lid
m2_pilot   = 1.70;   // self-tap pilot in plastic
cs_d       = 4.20;   // countersink diameter for M2 flat head (DIN 965, head 3.8)
cs_depth   = 1.05;
shcs_head_d = 3.8;   // M2 socket head
shcs_head_h = 2.0;
shcs_cb_d  = 4.4;    // counterbore for the head
shcs_seat_t = 1.5;   // seat under each head, standing off the inside of the lid
shcs_seat_d = 5.2;   // (flattened on the battery side; sized to clear the cavity's corners)

/* ---------------- mounts: magnets in the lid + magnetic accessories ---------------- */
// Two 6 x 3 mm round magnets are glued into shallow pockets in the lid's outer face, on its
// centreline, and stand proud of it. Each accessory (the spring clip, the strap plate) has two
// matching holes with its own magnets glued at the bottom: the lid's magnets slide into the
// holes and meet the accessory's face to face, so they also locate it. Each hole is stepped: a
// snug seat for the accessory's magnet, then a looser, chamfered bore for the lid's magnet to
// slide into. Set the lid's two with opposite poles out (N, S); stick each accessory magnet onto
// a lid magnet to mark its face, then glue it in with that marked face up, so both pairs attract
// (and the accessory only goes on one way round).
mag_d      = 6;      // round magnets: 6 mm dia x 3 mm
mag_t      = 3;
mag_fit    = 0.1;    // accessory magnet seat clearance, on the radius
mag_lid_fit = 0.2;   // lid magnet pocket clearance, on the radius
mag_slide  = 0.3;    // accessory bore clearance round the lid's magnets, on the radius
mag_lead   = 0.5;    // chamfer on the mouth of that bore
mag_floor  = 0.4;    // lid left behind its magnets
mag_dy     = 8.0;    // magnets at +/- mag_dy along the device, about mount_yc
mount_yc   = 3.0;    // centre of the magnet pattern along the device (Y)
acc_floor  = 0.6;    // accessory plastic behind its magnets

// spring clip: grips a handle running across the device. An upper jaw (magnets on top, a saddle
// round the top of the handle) and a lower C jaw pivot on a pin; a compression spring between
// their tails holds them shut -- squeeze the tails together to open it. Both jaws are lined with
// ribbed TPU liners, printed separately, that slide into dovetail grooves from the +X side up to
// a stop at the -X side.
//   Spring: 23/64" x 1-3/8" compression spring (9.1 mm OD x 34.9 mm long). The seats below
//   are sized from a 3/8" x 5/8" spring (spring_od / spring_len), which is too weak; the
//   longer spring fits the same seats.
//   Pivot pin: M2x20 screw + nut, a 2 mm pin, or a length of 1.75 mm filament.
handle_d   = 30;     // handle the clip grips
clip_w     = 22;     // clip width, along the handle (across the device)
clip_yc    = 5.0;    // where the handle sits along the device (Y)
ja_t       = 7.5;    // upper jaw thickness, along its tail and round the pivot
ja_seat    = 5.5;    // upper jaw thickness over the top of its saddle
sa_m       = 35;     // saddle spans this far round from the top of the handle, towards the mouth...
sa_p       = 40;     // ...and towards the pivot
sa_wall    = 3.0;    // saddle wall, beyond the liner
ja_u0      = -16;    // upper jaw's mouth-side end, from the handle centre
jaw_wall   = 4.0;    // lower (C) jaw thickness, beyond the liner
piv_u      = 16.5;   // pivot, along the device from the handle centre
piv_r      = 3.2;    // lower jaw knuckle radius
knuckle_w  = 10.6;   // lower jaw knuckle width, between the upper jaw's lugs
pin_d      = 2.2;    // pivot hole
tail_u1    = 32;     // both tails end here (along the device, from the handle centre)
spring_od  = 9.53;   // 3/8"
spring_len = 15.88;  // 5/8" free length
spring_pre = 3.4;    // compressed this much with the clip shut
pocket_a   = 3.0;    // spring pocket depth in the upper jaw's tail
pocket_b   = 4.0;    // ...and in the lower jaw's tail
// TPU grip liners
tpu_base   = 1.6;    // liner thickness behind the ribs
grip_h     = 0.5;    // grip ribs along the handle, standing this far off the liner
grip_pitch = 3.0;    // grip rib spacing round the handle
dv_n       = 3.0;    // dovetail keys on the liners' backs: neck width...
dv_w       = 4.6;    // ...head width...
dv_h       = 1.4;    // ...and depth
dv_fit     = 0.15;   // groove clearance, per face
dv_stop    = 1.5;    // grooves stop this far short of the jaws' -X side
kb_a       = [20, 100];   // lower jaw key angles (the upper jaw's sits between its magnets)

// strap plate: magnets on top, a sideways tunnel underneath for an elastic strap
strap_w    = 20.0;   // elastic strap width
strap_t    = 2.6;    // tunnel height
band_w     = 26;     // plate width (across the device)
band_len   = 27;     // plate length, along the device (centred on the magnets)
band_bot   = 1.6;    // floor under the tunnel

$fn = 64;

/* ---------------- derived ---------------- */
pocket_w = board_w + 2*fit;
pocket_l = board_l + 2*fit + far_extra;
pocket_r = board_r + fit;
cav_w    = bat_w + 2*bat_fit;        // cavity behind the board frame
outer_w  = cav_w + 2*wall;
usb_end_y = -(board_l/2 + fit + wall_end);   // outer USB end face
outer_r  = pocket_r + wall_end;
cav_l    = 2*(-usb_end_y) - 2*wall;
cav_r    = outer_r - wall;

// controls bay, along +Y
bay_iw   = outer_w - 2*bay_wall;
rib_y0   = board_l/2 + fit + far_extra;
rib_y1   = rib_y0 + rib_t;
rib_h    = frame_h + ledge_t;

z_front  = -rim_proud;
z_back   = bat_top + bat_gap;                // tub ends, lid starts
z_lid1   = z_back + lid_t;

wheel_z  = front_skin + 0.3 + enc_w/2;       // as close to the face as the encoder allows
// wheel size: the switch (and the button in front of it) sit below the wheel on the end face,
// so the tub depth sets how big the wheel can be
btn_zc_max = min(z_back - btn_hgt/2 - btn_clear - 0.75, z_back - sw_body/2 - 0.3);
wr_fit2  = min(pow(btn_zc_max - wheel_z - sw_body/2, 2) + pow(sw_body_h - enc_axis_h, 2),
               pow(btn_zc_max - wheel_z - btn_hgt/2, 2) + pow(sw_h + 0.1 - enc_axis_h, 2),
               pow(btn_zc_max - wheel_z - 1.0 - btn_hgt/2 - btn_clear, 2) + pow(wheel_z - end_t, 2));
wheel_d  = floor((sqrt(wr_fit2) - 0.4) * 20) / 10;   // to 0.1 mm
wheel_ribs = round(PI * wheel_d / rib_pitch);
wheel_r  = wheel_d/2;
wr_clear = wheel_r + 0.4;
wheel_y  = rib_y1 + max(wr_clear, wire_gap + pb_t + enc_axis_h);
pb_y1    = wheel_y - enc_axis_h;             // protoboard component face
pb_y0    = pb_y1 - pb_t;                     // protoboard back face
end_y1   = wheel_y + wheel_z;                // outer end face: as far from the axis as the display face
end_y0   = end_y1 - end_t;                   // inner end face
wheel_proud     = wheel_r - wheel_z;             // past the display face
wheel_proud_end = wheel_r - (end_y1 - wheel_y);  // past the end face
enc_x1   = -bay_iw/2 + 0.2 + enc_t;          // encoder's wheel-side face
wheel_x0 = enc_x1 + 0.5;
wheel_x1 = bay_iw/2 - 0.4;
// rounded top edge at the far end (display face into end face), following the wheel. Its
// radius is the largest that leaves top_skin of wall + top_enc_clear of space over the
// encoder body's corner, which sits ce in from the end face and cf in from the display face.
ce       = end_y1 - (wheel_y + enc_top);
cf       = wheel_z - enc_w/2 - z_front;
tr_s     = top_skin + top_enc_clear;
tr_k     = ce + cf - tr_s;
top_r    = floor((tr_k + sqrt(tr_k*tr_k - (ce*ce + cf*cf - tr_s*tr_s))) * 10) / 10;
slot_hl  = sqrt(wr_clear*wr_clear - pow(wheel_z - front_t, 2));             // face opening, behind the axis
end_open_z = wheel_z + sqrt(wr_clear*wr_clear - pow(end_y0 - wheel_y, 2));  // end opening depth

// switch height: as close to the wheel as the switch body allows, with a web below the opening
btn_zc   = max(wheel_z + sqrt(wr_clear*wr_clear - pow(sw_body_h - enc_axis_h, 2)) + sw_body/2,
               wheel_z + sqrt(wr_clear*wr_clear - pow(sw_h + 0.1 - enc_axis_h, 2)) + btn_hgt/2,
               end_open_z + 1.0 + btn_hgt/2 + btn_clear);
pin_y0   = pb_y1 + sw_h + 0.1 + btn_pin_trim;   // pin tip, just off the (real) plunger
pin_len  = end_y0 - btn_lip_t - pin_y0;
assert(btn_zc <= btn_zc_max + 1e-6, "button/switch don't fit below the wheel at this tub depth");

pb_x     = bay_iw/2 + groove_d - 0.1;        // protoboard half-width
pb_z0    = front_t;
pb_z1    = z_back - 0.15;
notch_z1 = wheel_z + sqrt(wr_clear*wr_clear - enc_axis_h*enc_axis_h);   // board notch for the wheel
notch_x0 = wheel_x0 - 0.4;

hook_x   = [for (sx=[-1,1]) sx*(bay_iw/2 - 0.2 - hook_w/2)];
hook_zb  = z_back - hook_h + 0.4;            // bottom of the barb
hook_gz0 = z_back - hook_h + 0.2;            // groove in the end wall
hook_gz1 = z_back - 0.6;

hole_pos = [for (sx=[-1,1], sy=[-1,1]) [sx*hole_dx/2, sy*hole_dy/2]];
shaft_len = outer_w/2 - (enc_x1 - key_len);
// lid screws: longest standard M2 that stays 0.5 mm clear of the ESP32's PCB. A socket
// screw's length is measured from under its head; a countersunk one's includes the head.
head_seat_z = z_lid1 - shcs_head_h;          // socket heads sit here, flush with the lid's outside
screw_under = lid_screw == "socket" ? head_seat_z : z_lid1;
screw_len = max([for (l=[6,8,10,12,14,16,18,20]) if (screw_under - l >= frame_h + 0.5) l]);

// mounts
mag_xy   = [for (sy=[-1,1]) [0, mount_yc + sy*mag_dy]];   // magnet centres
mag_proud = mag_t - (lid_t - mag_floor);      // how far the lid's magnets stand out of it
acc_slide = mag_proud + 0.2;                 // depth of the looser bore the lid's magnets slide into
acc_hole = acc_slide + mag_t;                // hole depth in the accessories (their magnet at the bottom)
band_top = acc_hole + acc_floor;             // strap plate: magnet layer above the tunnel
band_y0  = mount_yc - band_len/2;
band_y1  = mount_yc + band_len/2;
hr       = handle_d/2;
ja_top   = 0;                       // upper jaw's top face, on the lid
ja_bot   = ja_t;                    // its underside along the tail
piv_w    = ja_bot - 2.5;            // pivot depth (inside the upper jaw's lugs)
seat_r   = hr + grip_h + tpu_base;   // both jaws' seats, behind the liners
hc_w     = ja_seat + seat_r;        // handle centre depth (rib tips just touching it)
ka_a     = -90 + asin((mount_yc - clip_yc)/seat_r);   // upper jaw key: between the magnets
c_a0     = -35;  c_a1 = 135;        // lower jaw arc: from near the pivot, round the bottom, up the mouth side
                                    // (with sa_m 35: a 30 mm handle passes the mouth at ~14 deg open; the tails meet at ~21)
spring_u = (piv_u + piv_r + 1 + tail_u1) / 2;
spring_il = spring_len - spring_pre;
tb_top   = ja_bot + spring_il - pocket_a - pocket_b;   // lower jaw tail, top face
tb_bot   = tb_top + pocket_b + 2;
band_th  = band_top + strap_t + band_bot;
assert(mag_proud >= 0, "lid thicker than the magnets: use flush pockets instead");
assert(piv_w - piv_r - 0.3 >= ja_top + 1.4, "upper jaw too thin over the pivot");
assert(ja_seat - dv_h - dv_fit >= 2.5, "upper jaw too thin over its liner groove");
assert(ja_seat >= acc_hole + acc_floor - 0.5, "upper jaw too thin over the handle for the magnets");
assert(mag_dy - (mag_d/2 + mag_slide + mag_lead) - (dv_w/2 + dv_fit) >= 1.0, "upper liner groove runs into the magnet holes");
assert(seat_r*sin(sa_p) < piv_u - piv_r - 1.2 - 0.5, "upper liner runs into the knuckle slot");
assert(min([for (m=mag_xy, h=hole_pos) norm(m - h)]) > mag_d/2 + mag_lid_fit + shcs_cb_d/2 + 0.6,
       "magnets run into the screw holes");
assert(max([for (m=mag_xy) m[1]]) + mag_d/2 + mag_slide + mag_lead + 1.5 < clip_yc + piv_u - piv_r - 1.2, "clip magnets run into the pivot slot");
assert(min([for (m=mag_xy) m[1]]) - mag_d/2 - mag_slide - mag_lead - 1.5 > clip_yc + ja_u0, "clip magnets run off the upper jaw");

echo(str("Outer size: ", outer_w, " x ", end_y1 - usb_end_y, " x ", z_lid1 - z_front, " mm; far top edge R", top_r));
echo(str("Lid screws: 4x M2x", screw_len, lid_screw == "socket" ? " socket head cap" : " countersunk",
         " (", screw_len - (screw_under - board_h), " mm into the standoffs)"));
echo(str("Wheel: ", wheel_d, " dia x ", wheel_x1 - wheel_x0, " wide, sticks out ", wheel_proud,
         " (face) / ", wheel_proud_end, " (end); shaft ", shaft_len, " long"));
echo(str("Protoboard: ", 2*pb_x, " x ", pb_z1 - pb_z0, " mm; notch from the wheel-side corner ",
         pb_x - notch_x0, " x ", notch_z1 - pb_z0, " mm"));

/* ---------------- helpers ---------------- */
module rrect2d(w, l, r) offset(r) square([w-2*r, l-2*r], center=true);
module rrect(w, l, r, z0, z1) translate([0,0,z0]) linear_extrude(z1-z0) rrect2d(w, l, r);
module hex2d(af) circle(d=af/cos(30), $fn=6);   // flats parallel to X

// outer footprint: big radius at the USB end, small radius at the controls end
module outline2d(inset=0) hull() for (sx=[-1,1]) {
    translate([sx*(outer_w/2 - outer_r), usb_end_y + outer_r]) circle(r=outer_r - inset);
    translate([sx*(outer_w/2 - far_r), end_y1 - far_r]) circle(r=far_r - inset);
}
module outline(inset, z0, z1) translate([0,0,z0]) linear_extrude(z1-z0) outline2d(inset);

// footprint block with chamfered front and back edges
module chamfered_block(z0, z1, c0=edge_ch, c1=edge_ch) hull() {
    outline(c0, z0, z0+0.01);
    outline(0, z0+c0, z1-c1);
    outline(c1, z1-0.01, z1);
}

// countersunk screw hole; head flush with plane z_head, shank going toward -Z
module cs_hole(z_head, len=20, d=m2_clear) {
    translate([0,0,z_head-len]) cylinder(d=d, h=len+0.01);
    translate([0,0,z_head-cs_depth]) cylinder(d1=d-0.2, d2=cs_d, h=cs_depth+0.001);
    translate([0,0,z_head]) cylinder(d=cs_d, h=5);
}

// cylinder along +X
module xcyl(d, x0, x1) translate([x0,0,0]) rotate([0,90,0]) cylinder(d=d, h=x1-x0);
// hex prism along +X, flats facing +/-Z
module xhex(af, x0, x1) translate([x0,0,0]) rotate([0,90,0]) linear_extrude(x1-x0) rotate(90) hex2d(af);

/* ---------------- tub ---------------- */
module ledges() {
    translate([0,0,frame_h]) linear_extrude(ledge_t) difference() {
        rrect2d(cav_w+0.2, cav_l+0.2, cav_r);
        rrect2d(board_w - 2*ledge_w, board_l - 2*ledge_w, board_r - ledge_w);
        square([2*ledge_x, cav_l+1], center=true);
        square([cav_w+1, 2*ledge_y], center=true);
        for (p=hole_pos) translate(p) circle(r=standoff_r+ledge_sclear);
    }
}

// the material taken off by the far top round-over, grown inward by `inset`
module top_round(inset=0) translate([0, end_y1 - top_r, z_front + top_r]) difference() {
    translate([-outer_w, 0, -top_r - 1]) cube([2*outer_w, top_r + 1, top_r + 1]);
    xcyl(2*(top_r - inset), -outer_w, outer_w);
}

module tub() {
    difference() {
        union() {
            difference() {
                chamfered_block(z_front, z_back, edge_ch, 0.3);
                translate([0, far_extra/2, 0])                                   // display frame
                    rrect(pocket_w, pocket_l, pocket_r, z_front-1, frame_h+0.01);
                rrect(cav_w, cav_l, cav_r, frame_h, z_back+1);                  // PCB + battery
                // controls bay, and the wiring passage over the rib
                translate([-bay_iw/2, rib_y1, front_t]) cube([bay_iw, end_y0 - rib_y1, z_back]);
                translate([-bay_iw/2, rib_y0 - 1, rib_h]) cube([bay_iw, rib_t + 2, z_back]);
            }
            ledges();
        }

        // USB-C cutout (enclosed), sized for typical cable plug overmolds
        translate([0, usb_end_y - 1, (usb_z0+usb_z1)/2]) rotate([-90,0,0])
            linear_extrude(wall_end + 2.5) rrect2d(usb_cut_w, usb_cut_h, usb_cut_r);

        // protoboard grooves
        for (sx=[-1,1]) translate([sx*(bay_iw/2 + groove_d/2), (pb_y0+pb_y1)/2, (front_t + z_back + 1)/2])
            cube([groove_d + 0.02, pb_t + pb_fit, z_back + 1 - front_t], center=true);

        // round over the far top edge (display face into end face); the lid side stays square
        top_round(0);

        // corner opening for the wheel: through the display face and down the end wall.
        // Square corners -- the wheel's rims run within 0.4 mm of the sides, so any corner
        // radius would rub them.
        translate([(wheel_x0 + wheel_x1)/2, 0, 0]) {
            open_w = wheel_x1 - wheel_x0 + 0.8;
            y0 = wheel_y - slot_hl;  y1 = end_y1 + 4;
            translate([0, (y0 + y1)/2, z_front - 1])
                linear_extrude(front_t + 1.02 - z_front) square([open_w, y1 - y0], center=true);
            translate([0, end_y0 - 0.01, (z_front - 4 + end_open_z)/2]) rotate([-90,0,0])
                linear_extrude(end_t + 1) square([open_w, end_open_z - (z_front - 4)], center=true);
        }

        // pocket for the encoder body under the face plate (leaves front_skin), continuing as a
        // shallow channel in the end wall that is open to the back, so it slides in with the board
        // (trimmed back to stay top_skin inside the rounded top edge)
        difference() {
            translate([-bay_iw/2 - 0.01, pb_y1 - 0.01, front_skin])
                cube([enc_x1 + 0.3 + bay_iw/2, wheel_y + enc_top + 0.3 - pb_y1, z_back]);
            top_round(top_skin);
        }

        // button hole in the end face
        translate([0, end_y0 - 1, btn_zc]) rotate([-90,0,0])
            linear_extrude(end_t + 2) rrect2d(btn_w + 2*btn_clear, btn_hgt + 2*btn_clear, 1.2);

        // groove for the lid hooks
        for (x=hook_x) translate([x - hook_w/2 - 0.3, end_y0 - 0.01, hook_gz0])
            cube([hook_w + 0.6, hook_barb + 0.15 + 0.01, hook_gz1 - hook_gz0]);

        // wheel shaft: bearing hole + counterbore for the head, in the +X side wall
        translate([0, wheel_y, wheel_z]) {
            xcyl(shaft_d + 0.3, bay_iw/2 - 1, outer_w/2 + 1);
            xcyl(5.0, outer_w/2 - 0.8, outer_w/2 + 1);
        }
    }
}

/* ---------------- lid ---------------- */
module lid() {
    lip_h = 1.5; lip_t = 0.9; lip_gap = 0.2;
    difference() {
        union() {
            chamfered_block(z_back, z_lid1, 0.3, edge_ch);
            // locating lip at the USB end, beyond the battery
            intersection() {
                translate([0,0,z_back-lip_h]) linear_extrude(lip_h+0.01) difference() {
                    offset(-lip_gap) rrect2d(cav_w, cav_l, cav_r);
                    offset(-lip_gap-lip_t) rrect2d(cav_w, cav_l, cav_r);
                }
                translate([-outer_w, usb_end_y - 1, 0]) cube([2*outer_w, -usb_end_y + 1 - (bat_l/2 + 0.5), 50]);
            }
            // locating lips along both bay side walls
            for (sx=[-1,1]) translate([sx > 0 ? bay_iw/2 - lip_gap - lip_t : -bay_iw/2 + lip_gap,
                                       pb_y1 + 0.6, z_back - lip_h])
                cube([lip_t, end_y0 - 0.3 - hook_t - (pb_y1 + 0.6), lip_h + 0.01]);
            // far-end hooks: hang inside the end wall, barb catches the groove
            for (x=hook_x) translate([x - hook_w/2, 0, 0]) {
                translate([0, end_y0 - 0.15 - hook_t, z_back - hook_h]) cube([hook_w, hook_t, hook_h + 0.01]);
                hull() {   // barb, sloped underneath
                    translate([0, end_y0 - 0.16, hook_zb]) cube([hook_w, 0.01, hook_barb + 0.75]);
                    translate([0, end_y0 - 0.16, hook_zb + hook_barb + 0.15]) cube([hook_w, hook_barb + 0.16, 0.6]);
                }
            }
            // seats under the socket heads (the head takes the lid's full thickness)
            if (lid_screw == "socket" && shcs_seat_t > 0) for (p=hole_pos) translate([p[0], p[1], head_seat_z - shcs_seat_t])
                intersection() {
                    cylinder(d=shcs_seat_d, h=shcs_seat_t + 0.01);
                    // flat on the battery side: keep only |Y| >= bat_l/2 + 0.3
                    bat_edge = bat_l/2 + 0.3;
                    translate([-5, p[1] > 0 ? bat_edge - p[1] : -5, -1])
                        cube([10, p[1] > 0 ? 10 : (-bat_edge - p[1]) + 5, shcs_seat_t + 2]);
                }
        }
        for (p=hole_pos) translate([p[0], p[1], 0])
            if (lid_screw == "socket") {
                translate([0,0,z_back - shcs_seat_t - 1]) cylinder(d=m2_clear, h=lid_t + shcs_seat_t + 3);
                translate([0,0,head_seat_z]) cylinder(d=shcs_cb_d, h=lid_t + 1);
            } else cs_hole(z_lid1, len=lid_t+2);
        // mount magnets: pockets in the outer face (glue them in; they stand mag_proud out)
        for (m=mag_xy) translate([m[0], m[1], z_lid1 - (lid_t - mag_floor)])
            cylinder(d=mag_d + 2*mag_lid_fit, h=lid_t, $fn=48);
    }
}

/* ---------------- magnetic accessories ---------------- */
// magnet holes for any accessory, open at the contact face (z = z_lid1): a chamfered, looser
// bore the lid's magnets slide into, stepping down to a snug seat for the accessory's own magnet
module mount_magnet_pockets() for (m=mag_xy) translate([m[0], m[1], z_lid1]) {
    translate([0, 0, -0.01]) cylinder(d=mag_d + 2*mag_fit, h=acc_hole + 0.01, $fn=48);
    translate([0, 0, -0.01]) cylinder(d=mag_d + 2*mag_slide, h=acc_slide + 0.01, $fn=48);
    translate([0, 0, -0.01]) cylinder(d1=mag_d + 2*(mag_slide + mag_lead) + 0.02, d2=mag_d + 2*mag_slide,
                                      h=mag_lead + 0.01, $fn=48);
}
module lid_magnets_mock() color("silver") for (m=mag_xy) translate([m[0], m[1], z_lid1 - (lid_t - mag_floor)])
    cylinder(d=mag_d, h=mag_t, $fn=48);
module acc_magnets_mock() color("silver") for (m=mag_xy) translate([m[0], m[1], z_lid1 + acc_hole - mag_t])
    cylinder(d=mag_d, h=mag_t, $fn=48);

// Clip profiles are drawn in (u, w): u along the device from the handle centre, w away from
// the lid face. clip_place() turns that into case coordinates and extrudes along X.
module annulus_arc(c, r0, r1, a0, a1) translate(c) intersection() {
    difference() { circle(r=r1, $fn=128); circle(r=r0, $fn=128); }
    polygon(concat([[0,0]], [for (a=[a0:(a1-a0)/24:a1]) 3*r1*[cos(a), sin(a)]]));
}
module clip_place(x0, x1) translate([x0, clip_yc, z_lid1]) rotate([90,0,90]) linear_extrude(x1 - x0) children();

// dovetail key (or, with fit, its groove) on the seat circle at angle a, pointing away from the handle
module dovetail_2d(a, f=0) translate([0, hc_w]) rotate(a) translate([seat_r, 0])
    polygon([[-0.5, -dv_n/2 - f], [0, -dv_n/2 - f], [dv_h + f, -dv_w/2 - f],
             [dv_h + f, dv_w/2 + f], [0, dv_n/2 + f], [-0.5, dv_n/2 + f]]);
module liner_grooves(angles) clip_place(-clip_w/2 + dv_stop, clip_w) for (a=angles) dovetail_2d(a, dv_fit);
module wedge_2d(a0, a1, r) polygon(concat([[0,0]], [for (a=[a0:(a1-a0)/24:a1]) r*[cos(a), sin(a)]]));

// upper jaw: magnet holes on top, a saddle round the top of the handle, lugs round the pivot,
// spring pocket
module clip_a() difference() {
    clip_place(-clip_w/2, clip_w/2) difference() {
        union() {
            translate([ja_u0, ja_top]) square([tail_u1 - ja_u0, ja_t]);
            translate([0, hc_w]) intersection() {
                circle(r=seat_r + sa_wall, $fn=128);
                wedge_2d(-90 - sa_m, -90 + sa_p, 2*seat_r);
            }
        }
        translate([0, hc_w]) circle(r=seat_r, $fn=128);
    }
    mount_magnet_pockets();
    liner_grooves([ka_a]);
    // slot for the lower jaw's knuckle, open underneath
    clip_place(-knuckle_w/2 - 0.3, knuckle_w/2 + 0.3) {
        translate([piv_u, piv_w]) circle(r=piv_r + 0.35, $fn=48);
        translate([piv_u - piv_r - 1.2, piv_w]) square([2*piv_r + 1.55, 30]);   // extra room for the arm
    }
    clip_place(-clip_w, clip_w) translate([piv_u, piv_w]) circle(d=pin_d, $fn=24);       // pivot
    translate([0, clip_yc + spring_u, z_lid1 + ja_bot - pocket_a]) cylinder(d=spring_od + 0.7, h=pocket_a + 1);
}

// lower jaw: C round the bottom of the handle, knuckle on the pivot, tail with spring pocket
module clip_b() difference() {
    union() {
        clip_place(-clip_w/2, clip_w/2) union() {
            annulus_arc([0, hc_w], seat_r, seat_r + jaw_wall, c_a0, c_a1);
            translate([14, tb_top]) square([tail_u1 - 14, tb_bot - tb_top]);
        }
        // knuckle and the arm down to the C
        clip_place(-knuckle_w/2, knuckle_w/2) hull() {
            translate([piv_u, piv_w]) circle(r=piv_r, $fn=48);
            translate([0, hc_w] + (seat_r + jaw_wall/2)*[cos(c_a0), sin(c_a0)]) circle(r=jaw_wall/2, $fn=32);
        }
    }
    clip_place(-clip_w, clip_w) translate([0, hc_w]) circle(r=seat_r, $fn=128);         // seat, behind the liner
    liner_grooves(kb_a);
    clip_place(-clip_w, clip_w) translate([piv_u, piv_w]) circle(d=pin_d, $fn=24);       // pivot
    translate([0, clip_yc + spring_u, z_lid1 + tb_top - 1]) cylinder(d=spring_od + 0.7, h=pocket_b + 1);
}

// TPU liners: a ribbed arc on each jaw's seat, with dovetail keys on the back
module liner(a0, a1, keys) {
    n = floor((a1 - a0) / (grip_pitch/hr*180/PI));
    step = (a1 - a0) / n;
    clip_place(-clip_w/2, clip_w/2) translate([0, hc_w]) {
        annulus_arc([0, 0], hr + grip_h, seat_r, a0, a1);
        for (a=[a0 + step/2 : step : a1 - step/4]) rotate(a) translate([hr + grip_h + 0.1, 0]) circle(r=grip_h + 0.1, $fn=20);
    }
    clip_place(-clip_w/2 + dv_stop + 0.3, clip_w/2) for (a=keys) dovetail_2d(a);
}
module liner_a() liner(-90 - sa_m, -90 + sa_p, [ka_a]);
module liner_b() liner(c_a0, c_a1, kb_a);

module clip() { color("steelblue") clip_a(); color("slategray") clip_b(); color("darkorange") { liner_a(); liner_b(); }
                spring_mock(); pin_mock(); }
module spring_mock() color("silver") translate([0, clip_yc + spring_u, z_lid1 + ja_bot - pocket_a])
    difference() { cylinder(d=spring_od, h=spring_il, $fn=32); translate([0,0,-1]) cylinder(d=spring_od - 1.6, h=spring_il + 2, $fn=32); }
module pin_mock() color("silver") translate([0, clip_yc + piv_u, z_lid1 + piv_w]) xcyl(2.0, -clip_w/2, clip_w/2);
module handle_mock() color("tan") translate([-30, clip_yc, z_lid1 + hc_w]) xcyl(handle_d, 0, 60);

// strap plate: magnet holes on top, a sideways tunnel for the strap underneath
module band() difference() {
    union() {
        translate([0, 0, z_lid1]) hull() {
            translate([0, 0, 0.4]) linear_extrude(band_th - 0.8)
                translate([0, (band_y0 + band_y1)/2]) rrect2d(band_w, band_y1 - band_y0, 3);
            linear_extrude(band_th)
                translate([0, (band_y0 + band_y1)/2]) rrect2d(band_w - 0.8, band_y1 - band_y0 - 0.8, 2.6);
        }
    }
    mount_magnet_pockets();
    translate([-band_w, mount_yc - (strap_w + 0.8)/2, z_lid1 + band_top]) cube([2*band_w, strap_w + 0.8, strap_t]);
}
module strap_mock() color("black", 0.8)
    translate([-25, mount_yc - strap_w/2, z_lid1 + band_top + 0.4]) cube([50, strap_w, 1.6]);

// print orientations
module clip_a_print()   // top (magnet holes) on the bed, saddle up
    translate([0, -clip_yc, -z_lid1]) clip_a();
module clip_b_print()   // side profile on the bed
    translate([0, 0, clip_w/2]) rotate([0,-90,0]) translate([0, -clip_yc, -z_lid1]) clip_b();
module liner_print()   // TPU: side profile on the bed, keys down to it
    translate([0, 0, clip_w/2]) rotate([0,90,0]) translate([0, -clip_yc, -z_lid1]) children();
module band_print()     // underside on the bed, magnet holes up
    translate([0, 0, band_th]) rotate([180,0,0]) translate([0, -mount_yc, -z_lid1]) band();

/* ---------------- printed controls parts (modelled in place) ---------------- */
module wheel() {
    translate([0, wheel_y, wheel_z]) difference() {
        intersection() {
            xcyl(wheel_d, wheel_x0, wheel_x1);
            hull() {                                   // small chamfers on both rims
                xcyl(wheel_d - 1.0, wheel_x0, wheel_x1);
                xcyl(wheel_d, wheel_x0 + 0.5, wheel_x1 - 0.5);
            }
        }
        for (i=[0:wheel_ribs-1]) rotate([360*i/wheel_ribs, 0, 0])
            translate([wheel_x0 + 1.2, 0, wheel_r - groove_dp + groove_w/2]) rotate([0,90,0])
                cylinder(d=groove_w, h=wheel_x1 - wheel_x0 - 2.4, $fn=16);
        // bore: round clearance most of the way, snug hex at the encoder end that drives the wheel
        xcyl(pass_d, wheel_x0 + drive_len - 0.01, wheel_x1 + 1);
        xhex(drive_bore, wheel_x0 - 1, wheel_x0 + drive_len);
    }
}

module shaft() {   // prints standing on its head
    translate([0, wheel_y, wheel_z]) {
        xcyl(4.6, outer_w/2 - 0.8, outer_w/2);                       // head, in the counterbore
        xcyl(shaft_d, bay_iw/2 - 0.2, outer_w/2);                    // bearing section, in the side wall
        xhex(drive_af, wheel_x0 + 0.2, bay_iw/2 - 0.2 + 0.01);       // drive hex, through the wheel
        xhex(key_print, enc_x1 - key_len, wheel_x0 + 0.2 + 0.01);    // tip, into the encoder
    }
}

module button() {
    translate([0, 0, btn_zc]) rotate([-90,0,0]) {   // local z = case +Y
        // bar through the end wall, standing proud, chamfered round the pressing face
        hull() {
            translate([0,0,end_y0 - 0.01]) linear_extrude(end_t + btn_proud - btn_ch + 0.01)
                rrect2d(btn_w, btn_hgt, 1.0);
            translate([0,0,end_y1 + btn_proud - 0.01]) linear_extrude(0.01)
                rrect2d(btn_w - 2*btn_ch, btn_hgt - 2*btn_ch, 1.0 - btn_ch);
        }
        // retaining wings either side, inside the wall
        translate([0,0,end_y0 - btn_lip_t]) linear_extrude(btn_lip_t)
            rrect2d(btn_w + 2*btn_lip, btn_hgt, 1.0);
        // plunger pin back to the switch (if there's any length left; otherwise the wings press it)
        if (pin_len > 0.3) translate([0,0,pin_y0]) linear_extrude(pin_len + 0.01)
            square([btn_pin_w, btn_hgt], center=true);
    }
}

// fit test, printed the same way as the real parts (pins standing like the shaft, holes
// vertical like the wheel bore):
//  - top row: encoder tip pins. Set key_print to the largest one that goes into the
//    encoder's hole without forcing.
//  - bottom row: a drive_af pin and a plate of holes as deep as the wheel's snug section.
//    Set drive_bore to the smallest hole the pin pushes into; it must not turn once in.
// (1st round: drive_bore 2.80 fit best; tip pins up to 1.70 were all loose in the encoder.)
fit_keys  = [1.75, 1.80, 1.85, 1.90];
fit_bores = [2.60, 2.70, 2.80, 2.90, 3.00];
module fit_pin(af, h) {
    cylinder(d=af/cos(30) + 2.4, h=1.0);                  // small foot so it stays put
    linear_extrude(h) hex2d(af);
}
module fit_tips() {   // just the encoder tip pins
    for (i=[0:len(fit_keys)-1]) translate([i*10, 0, 0]) {
        translate([-4.5, -6, 0]) cube([9, 4.5, 1.0]);
        translate([0, -3.75, 0.99]) linear_extrude(0.6)
            text(str(fit_keys[i]), size=2.0, halign="center", valign="center");
        fit_pin(fit_keys[i], key_len + 3);
    }
}
module fit_test() {
    fit_tips();
    translate([45, -2, 0]) fit_pin(drive_af, drive_len + 4);
    translate([-5, -21, 0]) difference() {
        cube([len(fit_bores)*10, 13, drive_len]);
        for (i=[0:len(fit_bores)-1]) translate([5 + i*10, 0, 0]) {
            translate([0, 8.6, -1]) linear_extrude(drive_len + 2) hex2d(fit_bores[i]);
            translate([0, 3.2, drive_len - 0.5]) linear_extrude(1)
                text(str(fit_bores[i]), size=2.2, halign="center", valign="center");
        }
    }
}

/* ---------------- protoboard (shape to cut) ---------------- */
module protoboard2d() {   // in the board plane: X across, Y here = -case Z (display edge at the top)
    difference() {
        translate([-pb_x, -pb_z1]) square([2*pb_x, pb_z1 - pb_z0]);
        translate([notch_x0, -notch_z1]) square([pb_x - notch_x0 + 1, notch_z1 - pb_z0 + 1]);
    }
}
module template2d() {   // board outline (seen from the component side) with parts engraved
    difference() {
        protoboard2d();
        // encoder body outline + axis mark
        translate([enc_x1 - enc_t, -wheel_z - enc_w/2]) difference() {
            square([enc_t, enc_w]); translate([0.4,0.4]) square([enc_t-0.8, enc_w-0.8]);
        }
        translate([enc_x1 - enc_t - 0.01, -wheel_z - 0.2]) square([enc_t + 0.4, 0.4]);
        // switch body outline + centre
        translate([-sw_body/2, -btn_zc - sw_body/2]) difference() {
            square([sw_body, sw_body]); translate([0.4,0.4]) square([sw_body-0.8, sw_body-0.8]);
        }
        translate([0, -btn_zc]) circle(d=1.0);
    }
}
module protoboard_mock() color("darkgreen")
    translate([0, pb_y1, 0]) rotate([90,0,0]) linear_extrude(pb_t) mirror([0,1,0]) protoboard2d();

/* ---------------- reference models (for checking only) ---------------- */
module board_mock() {
    color("black") hull() {
        rrect(board_w-1.4, board_l-1.4, board_r-0.7, 0, 0.01);
        rrect(board_w, board_l, board_r, 0.7, frame_h);
    }
    color("gold") for (p=hole_pos) translate([p[0], p[1], frame_h])
        difference() { cylinder(r=standoff_r, h=board_h-frame_h); cylinder(r=1, h=10); }
    color("silver") translate([-usb_w/2, -board_l/2-usb_protrude, usb_z0])
        cube([usb_w, 7.4, usb_z1-usb_z0]);
}
module battery_mock() color("royalblue")
    translate([-bat_w/2, -bat_l/2, bat_top - bat_h]) cube([bat_w, bat_l, bat_h]);   // as measured
module screws_mock() color("silver")
    for (p=hole_pos) translate([p[0], p[1], 0]) {
        translate([0,0,screw_under-screw_len]) cylinder(d=2, h=screw_len);
        if (lid_screw == "socket") translate([0,0,head_seat_z]) cylinder(d=shcs_head_d, h=shcs_head_h);
        else translate([0,0,z_lid1-1.2]) cylinder(d1=2, d2=3.8, h=1.2);
    }
module encoder_mock() color("dimgray")
    translate([enc_x1 - enc_t, pb_y1, wheel_z - enc_w/2]) cube([enc_t, wheel_y + enc_top - pb_y1, enc_w]);
module switch_mock() {
    color("black") translate([-sw_body/2, pb_y1, btn_zc - sw_body/2]) cube([sw_body, sw_body_h, sw_body]);
    color("gray") translate([0, pb_y1, btn_zc]) rotate([-90,0,0]) cylinder(d=sw_plunger, h=sw_h);
}
module controls_mock() { protoboard_mock(); encoder_mock(); switch_mock(); }

// print orientations
module wheel_print() translate([0,0,wheel_x1]) rotate([0,90,0]) translate([0, -wheel_y, -wheel_z]) wheel();
module shaft_print()   // standing on its head, tip up (use a brim)
    translate([0,0,outer_w/2]) rotate([0,90,0]) translate([0, -wheel_y, -wheel_z]) shaft();
module button_print()   // on its side, so the layer lines run the way it slides
    translate([0, -end_y0, btn_zc + btn_hgt/2]) mirror([0,0,1]) button();
module lid_print() translate([0, 0, z_lid1]) mirror([0, 0, 1]) lid();   // outer face on the bed

/* ---------------- output ---------------- */
if (part == "tub") tub();                                              // print display-face down
else if (part == "lid") lid_print();       // outer face on the bed, no supports
else if (part == "clip_a") clip_a_print();  // upper jaw: top on the bed, saddle up
else if (part == "liner_a") liner_print() liner_a();   // TPU liner for the upper jaw
else if (part == "liner_b") liner_print() liner_b();   // TPU liner for the lower jaw
else if (part == "liner_a_asm") liner_a();
else if (part == "liner_b_asm") liner_b();
else if (part == "band") band_print();      // strap plate: underside on the bed, magnets up
else if (part == "band_asm") band();
else if (part == "clip_b") clip_b_print();  // lower jaw: side profile on the bed
else if (part == "clip_asm") clip();
else if (part == "clip_a_asm") clip_a();
else if (part == "clip_b_asm") clip_b();
else if (part == "spring") spring_mock();
else if (part == "handle") handle_mock();
else if (part == "mount") { color("lightgray") tub(); color("dimgray") lid(); screws_mock(); lid_magnets_mock();
                            color("orange") wheel(); color("red") button(); clip(); acc_magnets_mock(); handle_mock(); }
else if (part == "mount_band") { color("lightgray") tub(); color("dimgray") lid(); screws_mock(); lid_magnets_mock();
                            color("orange") wheel(); color("red") button(); color("steelblue") band(); acc_magnets_mock(); strap_mock(); }
else if (part == "lid_magnets") lid_magnets_mock();                                     // (checks)
else if (part == "acc_magnets") acc_magnets_mock();                                     // (checks)
else if (part == "wheel") wheel_print();
else if (part == "shaft") shaft_print();
else if (part == "button") button_print();
else if (part == "template") template2d();
else if (part == "fit_test") fit_test();
else if (part == "fit_tips") fit_tips();
else if (part == "lid_asm") lid();
else if (part == "board") board_mock();
else if (part == "battery") battery_mock();
else if (part == "screws") screws_mock();
else if (part == "controls") controls_mock();
else if (part == "wheel_asm") wheel();
else if (part == "shaft_asm") shaft();
else if (part == "button_asm") button();
else if (part == "section") intersection() {      // cut through the controls (x = 1)
    union() { color("lightgray") tub(); board_mock(); battery_mock(); color("dimgray") lid();
              controls_mock(); color("orange") wheel(); color("red") button(); }
    translate([1, -50, -10]) cube([50, 130, 50]);
}
else if (part == "print") {
    tub();
    translate([outer_w+8, 0, 0]) lid_print();
    translate([-outer_w-6, 0, 0]) wheel_print();
    translate([-outer_w-6, 22, 0]) shaft_print();
    translate([-outer_w-6, -22, 0]) button_print();
    translate([2*outer_w + 40, 0, 0]) clip_a_print();
    translate([2*outer_w + 40, 60, 0]) clip_b_print();
    translate([2*outer_w + 40, 110, 0]) liner_print() liner_a();
    translate([2*outer_w + 70, 110, 0]) liner_print() liner_b();
    translate([2*outer_w + 80, 0, 0]) band_print();
}
else if (part == "exploded") {
    color("lightgray") tub();
    translate([0,0,-18]) board_mock();
    translate([0,0,16]) battery_mock();
    color("orange") translate([0,8,-10]) wheel();
    color("red") translate([0,8,0]) button();
    translate([0,0,28]) controls_mock();
    color("orange") translate([16,0,0]) shaft();
    color("dimgray") translate([0,0,44]) lid();
    translate([0,0,60]) screws_mock();
    translate([0,0,70]) clip();
}
else {  // assembled
    color("lightgray") tub();
    board_mock();
    battery_mock();
    controls_mock();
    color("orange") { wheel(); shaft(); }
    color("red") button();
    color("dimgray") lid();
    screws_mock();
}
