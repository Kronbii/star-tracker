// Barn-door star tracker - parametric OpenSCAD model.
// Hardware licence: CERN-OHL-P-2.0 (see LICENSE-HARDWARE).
//
//   openscad -D 'part="carriage"' -o stl/carriage.stl star_tracker.scad
//   ./build.sh            exports every printable part, the drill templates and renders
//
// Frame (assembly): the hinge axis (= polar axis) is the y axis through the
// origin; +y points to the celestial pole (north). The boards extend along +x
// (east when the boards are level). The top board turns about the hinge by
// theta; the firmware geometry is
//     R = radius (hinge axis to roller/screw line), c = roller radius - strike offset.
// Here the strike strip's contact face lies in the plane of the hinge axis
// (offset 0), so c = 11 mm for a 608 roller, matching the firmware default.

part = "assembly";      // which part to build (see the list at the bottom)
theta = 12;             // board angle for the assembly render, degrees
show_camera = true;     // camera + ball head stand-ins in the assembly
$fn = 72;

// ---------------------------------------------------------------- parameters
R = 200;                // hinge axis to lead-screw axis (mm) - firmware "radius"
roller_od = 22;         // 608ZZ used as the roller -> c = 11
board_t = 12;           // plywood thickness
hinge_h = 40;           // hinge axis height above the bottom board's top face
top_lift = 20;          // top board underside above the hinge axis (rotating frame)
h_home = 3;             // roller-centre height at the home switch - firmware "home"
h_max = 150;            // soft limit - firmware "max"

bot_x = [-30, 245];     // bottom board extent along x
bot_y = [-60, 135];     // ... along y
top_x = [-15, 275];     // top board extent along its own x
top_y = [-55, 55];

screw_y = 100;          // lead-screw axis is at (R, screw_y)
rod_dx = 30;            // smooth rods at R +- rod_dx
rod_d = 8;
lm8_d = 15; lm8_l = 24; // LM8UU
nema = 42.3; nema_holes = 31; nema_boss = 22.5;
b608_od = 22; b608_w = 7; b608_id = 8;
roller_y = 67;          // roller centre (along the axle)
strip_w = 10; strip_t = 2;  // steel flat bar 10 x 2 on the strike bracket
strip_y = [61.5, 71.5];

sight_len = 200;        // pinhole-to-reticle distance
sight_tube_od = 16;     // aluminium tube or 16 mm electrical conduit
polaris_sep = 0.62;     // degrees from the pole, 2026-2027
sight_x = 35;           // tube position along the top board
lens_od = 70.4;         // Bahtinov mask: lens front barrel diameter + 0.4 clearance
lens_f = 200;

tol = 0.25;             // printing clearance for fits

// derived
base_top = -hinge_h + 18;           // drive base top surface (z)
cap_z = 194;                        // top cap underside
car_dn = 16; car_up = 14;           // carriage extent below/above the roller axle

// ---------------------------------------------------------------- helpers
module rcube(size, r = 3, center = false) {
    // rounded-edge (in xy) box
    translate(center ? -size / 2 : [0, 0, 0])
        hull() for (x = [r, size[0] - r], y = [r, size[1] - r])
            translate([x, y, 0]) cylinder(r = r, h = size[2]);
}
module hole(d, h, z0 = -1) { translate([0, 0, z0]) cylinder(d = d, h = h); }
module m3_hex_trap(depth = 3) { cylinder(d = 6.4, h = depth, $fn = 6); }

// ---------------------------------------------------------------- printed parts

// Hinge bearing block (x2): a 608 bearing for the 8 mm hinge shaft.
module hinge_block() {
    w = 40; t = 14;
    difference() {
        union() {
            translate([-w / 2, 0, 0]) rcube([w, t, 6], 2);           // foot
            hull() {
                translate([-14, 0, 0]) cube([28, t, 1]);
                translate([0, 0, hinge_h]) rotate([-90, 0, 0]) cylinder(d = b608_od + 8, h = t);
            }
        }
        translate([0, -1, hinge_h]) rotate([-90, 0, 0]) cylinder(d = b608_od + tol, h = b608_w + 1);
        translate([0, -1, hinge_h]) rotate([-90, 0, 0]) cylinder(d = 12, h = t + 2);
        for (x = [-14.5, 14.5]) translate([x, t / 2, 0]) {
            hole(5.3, 20);
            translate([0, 0, 5]) cylinder(d = 10, h = 30);
        }
    }
}

// Hinge shaft clamp (x2): grips the shaft, screws to the top board underside.
// Printed upside down (board face on the bed).
module hinge_clamp() {
    w = 16;
    difference() {
        hull() {
            translate([-14, 0, top_lift - 1]) cube([28, w, 1]);
            rotate([-90, 0, 0]) cylinder(d = 20, h = w);
        }
        translate([0, -1, 0]) rotate([-90, 0, 0]) cylinder(d = b608_id + 0.15, h = w + 2);
        translate([-0.75, -1, -12]) cube([1.5, w + 2, 12]);                 // pinch slot
        translate([-15, w / 2, -6]) rotate([0, 90, 0]) cylinder(d = 3.4, h = 30);
        translate([7, w / 2, -6]) rotate([0, 90, 0]) cylinder(d = 6.4, h = 10, $fn = 6);
        for (x = [-9, 9]) translate([x, w / 2, 4]) {
            cylinder(d = 4.4, h = 30);
            translate([0, 0, -10]) cylinder(d = 8.5, h = 10 + top_lift - 8);
        }
    }
}

// Drive base: rod sockets, motor bolt counterbores, coupler bore, home switch.
module drive_base() {
    L = 84; W = 40; H = base_top + hinge_h;    // 18 mm
    difference() {
        translate([-L / 2, -W / 2, 0]) rcube([L, W, H], 4);
        hole(30, H + 2);                                          // coupler
        for (s = [-1, 1]) translate([s * rod_dx, 0, 2]) {
            cylinder(d = rod_d + 0.1, h = H);                     // rod socket
            // M3 set screw from the back face (heat-set insert)
            translate([0, 0, H / 2]) rotate([-90, 0, 0]) cylinder(d = 4.0, h = W);
        }
        for (x = [-1, 1], y = [-1, 1])                            // NEMA17 bolts through the board
            translate([x * nema_holes / 2, y * nema_holes / 2, 0]) {
                hole(3.4, H + 2);
                translate([0, 0, H - 4]) cylinder(d = 6.2, h = 10);
            }
        for (x = [-36, 36]) translate([x, 13, 0]) {               // base-to-board screws
            hole(4.4, H + 2);
            translate([0, 0, H - 4]) cylinder(d = 8.5, h = 10);
        }
        // KW12 micro switch pocket (lever up) at the front, clear of the coupler bore;
        // mounting holes slotted +-2 mm in height to set the home position
        translate([-32, -W / 2 + 2, H - 8]) cube([20.4, 6.6, 10]);
        for (dx = [5.4, 14.9]) translate([-32 + dx, -W / 2 - 1, H - 4.5]) rotate([-90, 0, 0])
            hull() for (dz = [-2, 2]) translate([0, dz, 0]) cylinder(d = 2.4, h = 12);
    }
}

// Top cap: screw top bearing (608) and rod sockets.
module drive_cap() {
    L = 84; W = 30; H = 20;
    difference() {
        translate([-L / 2, -W / 2, 0]) rcube([L, W, H], 4);
        translate([0, 0, 6]) cylinder(d = b608_od + tol, h = H);  // bearing from the top
        hole(12, H + 2);
        for (s = [-1, 1]) translate([s * rod_dx, 0, -1]) {
            cylinder(d = rod_d + 0.1, h = 17);
            translate([0, 0, 8]) rotate([-90, 0, 0]) cylinder(d = 4.0, h = W);   // set screw
        }
    }
}

// Carriage: two LM8UU, floating Tr8 nut, and the side arm carrying the 608 roller
// on an M8 x 25 bolt. Origin = roller axle; the screw axis is at +y (screw_y - roller_y).
module carriage() {
    dy = screw_y - roller_y;     // 32: axle origin to screw axis
    L = 84; W = 24; H = car_dn + car_up;
    difference() {
        union() {
            translate([-L / 2, dy - W / 2, -car_dn]) rcube([L, W, H], 4);
            // arm/boss towards the roller
            hull() {
                translate([0, 6, 0]) rotate([-90, 0, 0]) cylinder(d = 18, h = 1);
                translate([-12, dy - W / 2, -car_dn]) cube([24, 1, H]);
            }
            // home flag under the carriage, presses the switch lever
            translate([-26, dy - W / 2 - 6, -car_dn - 6]) cube([8, 6, 6 + car_dn]);
        }
        // rods / LM8UU
        for (s = [-1, 1]) translate([s * rod_dx, dy, -car_dn - 1]) {
            cylinder(d = rod_d + 2, h = H + 2);
            translate([0, 0, (H + 2 - lm8_l) / 2]) cylinder(d = lm8_d + 0.15, h = lm8_l);
        }
        // lead screw + nut barrel; flange screws in 1.2 mm oversize holes = floating nut
        translate([0, dy, -car_dn - 1]) cylinder(d = 11, h = H + 2);
        for (a = [45:90:315]) translate([8 * cos(a), dy + 8 * sin(a), -car_dn - 1])
            cylinder(d = 4.2, h = H + 2);
        // roller axle M8 and its nut trap (open to the top)
        translate([0, -1, 0]) rotate([-90, 0, 0]) cylinder(d = 8.3, h = 26);
        translate([0, 14, 0]) rotate([-90, 0, 0]) rotate([0, 0, 30]) cylinder(d = 15, h = 7, $fn = 6);
        translate([-6.5, 14, 0]) cube([13, 7, 20]);
    }
}

// Strike bracket: holds the steel strip whose underside is the contact plane.
// Bolts to the +y edge and the top face of the top board. Origin: board corner.
module strike_bracket() {
    L = 92;   // along the board x (covers contact points from 4 to 39 deg)
    difference() {
        union() {
            translate([0, 0, strip_t - 1]) cube([L, strip_y[1] - top_y[1], top_lift - strip_t + 1]);
            translate([0, -16, top_lift + board_t]) cube([L, strip_y[1] - top_y[1] + 16, 5]);
            translate([0, 0, top_lift]) cube([L, 6, board_t]);
        }
        // strip recess (strip glued + 2 countersunk M3)
        // strip recess, 1 mm deep: a 2 mm strip glued in protrudes 1 mm, its
        // underside landing in the plane of the hinge axis
        translate([-1, strip_y[0] - top_y[1] - 0.2, strip_t - 1.01]) cube([L + 2, strip_w + 0.4, 1.01]);
        // M3 screws at the strip ends, outside the roller's 20..70 mm contact range
        for (x = [4, L - 4]) translate([x, (strip_y[0] + strip_y[1]) / 2 - top_y[1], -1])
            cylinder(d = 3.4, h = top_lift + 20);
        for (x = [20, L / 2, L - 20]) translate([x, -8, top_lift]) {
            cylinder(d = 4.4, h = 30);
            translate([0, 0, board_t + 3]) cylinder(d = 8.5, h = 5);
        }
        // relief so the board edge sits flush
        translate([-1, -16.01, top_lift - 0.01]) cube([L + 2, 16, board_t + 0.02]);
    }
}

// Sight tube cradles. The tube lies along y, parallel to the hinge axis.
module sight_cradle(front = false) {
    t = 14; hgt = 18; w = 36;
    bore = front ? sight_tube_od + 6 : sight_tube_od + tol;
    difference() {
        union() {
            translate([-w / 2, 0, 0]) rcube([w, t, 5], 2);
            translate([0, 0, hgt]) rotate([-90, 0, 0]) cylinder(d = bore + 10, h = t);
            translate([-(bore + 10) / 2, 0, 0]) cube([bore + 10, t, hgt]);
        }
        translate([0, -1, hgt]) rotate([-90, 0, 0]) cylinder(d = bore, h = t + 2);
        if (front) {
            // three M3 collimation screws at 120 degrees (heat-set inserts)
            for (a = [90, 210, 330]) translate([0, t / 2, hgt]) rotate([0, -a, 0])
                translate([0, 0, bore / 2 - 1]) cylinder(d = 4.0, h = 10);
        } else {
            translate([-0.75, -1, hgt]) cube([1.5, t + 2, 20]);
            translate([-20, t / 2, hgt + bore / 2 + 2.5]) rotate([0, 90, 0]) cylinder(d = 3.4, h = 40);
        }
        for (x = [-w / 2 + 5, w / 2 - 5]) translate([x, t / 2, 0]) hole(4.4, 10);
    }
}

// Eyepiece cap: 1.5 mm pinhole (eye end, south).
module sight_cap_eye() {
    difference() {
        cylinder(d = sight_tube_od + 4, h = 14);
        translate([0, 0, 2]) cylinder(d = sight_tube_od + tol, h = 20);
        hole(1.5, 5);
        translate([0, 0, -0.01]) cylinder(d1 = 8, d2 = 1.5, h = 1.6);
    }
}

// Reticle cap (north end): a circular aperture whose edge is Polaris'
// distance from the pole, seen from the pinhole sight_len away.
module sight_cap_reticle() {
    r_ring = sight_len * tan(polaris_sep);
    difference() {
        cylinder(d = sight_tube_od + 4, h = 14);
        translate([0, 0, 0.8]) cylinder(d = sight_tube_od + tol, h = 20);
        hole(2 * r_ring, 5, -1, $fn = 180);
        // clock ticks at 12/3/6/9 (outside the ring, half depth)
        for (a = [0:90:270]) rotate([0, 0, a]) translate([r_ring + 0.6, -0.3, 0.4]) cube([1.6, 0.6, 1]);
    }
}

// Electronics enclosure: perfboard 70x50 (ESP32 DevKitC, TMC2209, buck, 2x PC817).
module electronics_box() {
    L = 108; W = 72; H = 36; wall = 2.4;
    difference() {
        rcube([L, W, H], 5);
        translate([wall, wall, wall]) rcube([L - 2 * wall, W - 2 * wall, H], 3);
        translate([-1, 14, 14]) rotate([0, 90, 0]) cylinder(d = 8.2, h = 5);     // DC jack
        translate([-1, 30, 14]) rotate([0, 90, 0]) cylinder(d = 6.4, h = 5);     // power switch
        translate([-1, 48, 14]) rotate([0, 90, 0]) cylinder(d = 6.2, h = 5);     // 2.5 mm jack
        translate([L - 4, 14, 10]) cube([6, 10, 7]);                             // motor cable
        translate([L - 4, 34, 10]) cube([6, 6, 5]);                              // switch cable
        translate([L - 4, 50, 12]) cube([6, 13, 8]);                             // USB (flashing)
        for (x = [12, L - 12]) translate([x, W / 2, -1]) cylinder(d = 4.4, h = 4);   // to board
    }
    // perfboard standoffs (holes 64 x 44 for a 70 x 50 board)
    for (x = [0, 64], y = [0, 44]) translate([22 + x, 14 + y, 0]) difference() {
        cylinder(d = 7, h = 8);
        cylinder(d = 2.6, h = 9);
    }
    for (x = [5, L - 5], y = [5, W - 5]) translate([x, y, 0]) difference() {
        cylinder(d = 7, h = H);
        translate([0, 0, H - 10]) cylinder(d = 2.6, h = 11);
    }
}

module electronics_lid() {
    L = 108; W = 72;
    difference() {
        union() {
            rcube([L, W, 2], 5);
            translate([2.6, 2.6, 2]) difference() {
                rcube([L - 5.2, W - 5.2, 2], 3);
                translate([1.6, 1.6, -1]) rcube([L - 8.4, W - 8.4, 4], 2);
            }
        }
        for (x = [5, L - 5], y = [5, W - 5]) translate([x, y, -1]) cylinder(d = 3.2, h = 5);
        for (i = [0:5]) translate([30 + i * 9, 20, -1]) cube([4, 32, 5]);    // vents over the driver
    }
}

// Bahtinov focusing mask for the camera lens. Printed flat.
module bahtinov_mask() {
    ring_h = 12; wall = 1.6; t = 1.2;
    r = lens_od / 2;
    pitch = max(2.0, lens_f / 100);     // slot + bar; >= 2 mm so it prints
    slot = pitch / 2;
    rr = r - 1.5;
    module grating(angle) {
        rotate([0, 0, angle]) for (i = [-ceil(r / pitch):ceil(r / pitch)])
            translate([-r - 2, i * pitch - slot / 2, -1]) cube([2 * r + 4, slot, t + 2]);
    }
    difference() {
        cylinder(r = r + wall, h = t);
        intersection() {
            translate([-rr, -rr, -2]) cube([rr - 1, 2 * rr, t + 4]);
            cylinder(r = rr, h = t + 2, center = false);
            grating(90);
        }
        intersection() {
            translate([1, 1, -2]) cube([rr, rr, t + 4]);
            cylinder(r = rr, h = t + 2);
            grating(90 - 20);
        }
        intersection() {
            translate([1, -rr - 1, -2]) cube([rr, rr, t + 4]);
            cylinder(r = rr, h = t + 2);
            grating(90 + 20);
        }
    }
    difference() {
        cylinder(r = r + wall, h = ring_h);
        translate([0, 0, -1]) cylinder(r = r, h = ring_h + 2);
    }
    // handle tab
    translate([r - 1, -6, 0]) cube([10, 12, t]);
}

// ---------------------------------------------------------------- 2D drill templates (print 1:1)
module template_bottom() {
    difference() {
        translate([bot_x[0], bot_y[0]]) square([bot_x[1] - bot_x[0], bot_y[1] - bot_y[0]]);
        translate([bot_x[0] + 0.5, bot_y[0] + 0.5])
            square([bot_x[1] - bot_x[0] - 1, bot_y[1] - bot_y[0] - 1]);
    }
    // hinge blocks
    for (y = [-55, 41]) for (x = [-14.5, 14.5]) translate([x, y + 7]) mark(5);
    // drive: coupler/motor boss, motor bolts, base screws
    translate([R, screw_y]) mark(25);
    for (x = [-1, 1], y = [-1, 1]) translate([R + x * nema_holes / 2, screw_y + y * nema_holes / 2]) mark(3.5);
    for (x = [-36, 36]) translate([R + x, screw_y + 13]) mark(4);
    // tripod T-nut (3/8"-16), near the loaded centre of mass
    translate([110, 30]) mark(12);
    // electronics box
    for (x = [12, 96]) translate([45 + x, 60 + 36]) mark(4);
    // hinge axis line
    translate([-0.25, bot_y[0]]) square([0.5, bot_y[1] - bot_y[0]]);
    translate([R - 0.25, bot_y[0]]) square([0.5, bot_y[1] - bot_y[0]]);
}
module template_top() {
    difference() {
        translate([top_x[0], top_y[0]]) square([top_x[1] - top_x[0], top_y[1] - top_y[0]]);
        translate([top_x[0] + 0.5, top_y[0] + 0.5])
            square([top_x[1] - top_x[0] - 1, top_y[1] - top_y[0] - 1]);
    }
    for (y = [-38, 22]) for (x = [-9, 9]) translate([x, y + 8]) mark(3);      // hinge clamps
    translate([150, 0]) mark(6.5);                                           // 1/4"-20 ball head
    for (x = [20, 46, 72]) translate([180 + x, top_y[1] - 8]) mark(3);       // strike bracket
    for (y = [-84, 70]) for (x = [-13, 13]) translate([sight_x + x, y + 7]) mark(3);  // cradles
}
module mark(d) {
    difference() { circle(d = d); circle(d = max(d - 1, 0.3)); }
    square([d + 6, 0.3], center = true);
    square([0.3, d + 6], center = true);
}

// ---------------------------------------------------------------- vitamins (non-printed, for renders)
module steel() { color("#9aa3a0") children(); }
module dark() { color("#2b2f2d") children(); }
module wood() { color("#c9a978") children(); }
module print_c() { color("#b7d3a8") children(); }
module accent() { color("#d08fc6") children(); }

module nema17() {
    dark() translate([-nema / 2, -nema / 2, -40]) rcube([nema, nema, 40], 4);
    steel() translate([0, 0, 0]) cylinder(d = 5, h = 24);
    steel() cylinder(d = nema_boss, h = 2);
}

// ---------------------------------------------------------------- assembly
function h_for(th) = (R * sin(th) - roller_od / 2) / cos(th);

module top_frame(th) {
    // rotation about -y by th: +x rises towards +z
    rotate([0, -th, 0]) children();
}

module assembly(th = theta) {
    h = h_for(th);
    // bottom board
    wood() translate([bot_x[0], bot_y[0], -hinge_h - board_t])
        cube([bot_x[1] - bot_x[0], bot_y[1] - bot_y[0], board_t]);
    // hinge
    for (y = [-55, 41]) print_c() translate([0, y, -hinge_h]) hinge_block();
    steel() translate([0, -65, 0]) rotate([-90, 0, 0]) cylinder(d = 8, h = 130);
    // drive tower
    print_c() translate([R, screw_y, -hinge_h]) drive_base();
    translate([R, screw_y, -hinge_h - board_t]) nema17();
    steel() translate([R, screw_y, -hinge_h]) cylinder(d = 19, h = 25);           // coupler
    steel() translate([R, screw_y, -hinge_h + 12]) cylinder(d = 8, h = 250);      // Tr8x2
    for (s = [-1, 1]) steel() translate([R + s * rod_dx, screw_y, -hinge_h + 2]) cylinder(d = rod_d, h = 250);
    print_c() translate([R, screw_y, cap_z]) drive_cap();
    print_c() translate([R, roller_y, h]) carriage();
    steel() translate([R, roller_y - b608_w / 2, h]) rotate([-90, 0, 0])
        difference() { cylinder(d = roller_od, h = b608_w); hole(8, 9); }
    dark() translate([R, roller_y - b608_w / 2 - 8.6, h]) rotate([-90, 0, 0]) cylinder(d = 13, h = 8);
    // electronics
    accent() translate([45, 60, -hinge_h]) electronics_box();
    // top board and everything on it
    top_frame(th) {
        wood() translate([top_x[0], top_y[0], top_lift])
            cube([top_x[1] - top_x[0], top_y[1] - top_y[0], board_t]);
        for (y = [-38, 22]) print_c() translate([0, y, 0]) hinge_clamp();
        print_c() translate([180, top_y[1], 0]) strike_bracket();
        steel() translate([180, strip_y[0], 0]) cube([92, strip_w, strip_t]);
        // sight tube
        z0 = top_lift + board_t;
        print_c() translate([sight_x, -84, z0]) sight_cradle(false);
        print_c() translate([sight_x, 70, z0]) sight_cradle(true);
        steel() translate([sight_x, -sight_len / 2 + 1, z0 + 18]) rotate([-90, 0, 0])
            cylinder(d = sight_tube_od, h = sight_len - 2);
        print_c() translate([sight_x, -sight_len / 2 - 1, z0 + 18]) rotate([-90, 0, 0]) sight_cap_eye();
        print_c() translate([sight_x, sight_len / 2 + 0.8, z0 + 18]) rotate([90, 0, 0]) sight_cap_reticle();
        // ball head + camera stand-in
        if (show_camera) {
        dark() translate([150, 0, z0]) cylinder(d = 40, h = 30);
        dark() translate([150, 0, z0 + 42]) sphere(d = 30);
        translate([150, 0, z0 + 62]) rotate([0, 0, 0]) {
            dark() translate([-65, -40, 0]) rcube([130, 80, 95], 10);
            dark() translate([20, 0, 47]) rotate([0, 90, 0]) cylinder(d = 70, h = 120);
        }
        }
    }
}

// Contact check: roller, carriage and strike strip only (inspect along +y).
module contact_check(th) {
    h = h_for(th);
    print_c() translate([R, roller_y, h]) carriage();
    steel() translate([R, roller_y - b608_w / 2, h]) rotate([-90, 0, 0])
        difference() { cylinder(d = roller_od, h = b608_w); hole(8, 9); }
    dark() translate([R, roller_y - b608_w / 2 - 8.6, h]) rotate([-90, 0, 0]) cylinder(d = 13, h = 8);
    print_c() translate([R, screw_y, -hinge_h]) drive_base();
    top_frame(th) {
        wood() translate([150, top_y[0], top_lift]) cube([top_x[1] - 150, top_y[1] - top_y[0], board_t]);
        print_c() translate([180, top_y[1], 0]) strike_bracket();
        steel() translate([180, strip_y[0], 0]) cube([92, strip_w, strip_t]);
    }
}

// ---------------------------------------------------------------- part selector
if (part == "assembly") assembly(theta);
else if (part == "assembly_home") assembly(atan2(h_home, R) + asin(roller_od / 2 / sqrt(R * R + h_home * h_home)));
else if (part == "assembly_max") assembly(atan2(h_max, R) + asin(roller_od / 2 / sqrt(R * R + h_max * h_max)));
else if (part == "contact_home") contact_check(atan2(h_home, R) + asin(roller_od / 2 / sqrt(R * R + h_home * h_home)));
else if (part == "contact_max") contact_check(atan2(h_max, R) + asin(roller_od / 2 / sqrt(R * R + h_max * h_max)));
else if (part == "hinge_block") rotate([90, 0, 0]) hinge_block();   // bearing bore vertical
else if (part == "hinge_clamp") rotate([180, 0, 0]) translate([0, 0, -top_lift]) hinge_clamp();
else if (part == "drive_base") drive_base();
else if (part == "drive_cap") drive_cap();
else if (part == "carriage") translate([0, 0, car_up]) rotate([180, 0, 0]) carriage();  // top face down
else if (part == "strike_bracket") rotate([180, 0, 0]) translate([0, 0, -(top_lift + board_t + 5)]) strike_bracket();
else if (part == "sight_cradle_rear") sight_cradle(false);
else if (part == "sight_cradle_front") sight_cradle(true);
else if (part == "sight_cap_eye") sight_cap_eye();
else if (part == "sight_cap_reticle") sight_cap_reticle();
else if (part == "electronics_box") electronics_box();
else if (part == "electronics_lid") electronics_lid();
else if (part == "bahtinov_mask") bahtinov_mask();
else if (part == "template_bottom") template_bottom();
else if (part == "template_top") template_top();
