// ============================================================================
//  DotUhr – Retro-Gehäuse für 4-in-1 MAX7219 Matrix (32x8) + ESP32-C3 Super Mini
//  Stromanschluss (USB-C des ESP32) hinten mittig.
//
//  Koordinaten:  X = Breite,  Y = Tiefe (Front bei y=0),  Z = Höhe (Mitte = 0)
//  Teile:  "body" (Gehäuse), "cover" (Rückdeckel),
//          "diffuser" (Streuscheibe, optional), "assembly" (Gesamtansicht)
//  Export: openscad -D 'part="body"' -o body.stl dotuhr_gehaeuse.scad
// ============================================================================

part = "assembly";        // [assembly, body, cover, diffuser, print_all]

/* [Display-Modul] */
mod_w        = 132.0;     // Platinenlänge (Datenblatt: 13,2 cm)
mod_h        = 32.0;      // Platinenhöhe
mod_pcb_back = 10.0;      // Vorderseite Matrix -> Rückseite Platine (bitte nachmessen!)
post_short   = 4.0;       // Andruck-Stützen im Deckel um diesen Wert kürzer (Luft zum Modul)
post_from_right = [35, 100]; // Stützen: Abstand vom rechten Platinenrand (von vorne gesehen), mittig in der Höhe
cable_slot   = 20;        // Kabelschlitz links/rechts in der Displaytasche (Höhe, mittig)
mod_cols     = 32;        // Pixel horizontal (4 Module à 8)
pitch        = 4.0;       // LED-Raster (32 mm / 8)
tol          = 0.4;       // Spiel für Taschen

/* [Gehäuse] */
W      = 170;             // Außenbreite
H      = 62;              // Außenhöhe
D      = 60;              // Außentiefe gesamt (inkl. Deckel)
R      = 10;              // Eckenradius (Profil)
t      = 3.0;             // Wandstärke Seiten/Oben/Unten
tf     = 3.0;             // Wandstärke Front
back_t = 3.0;             // Deckelstärke
ch     = 1.5;             // Fase an Front-/Rückkante

/* [Fenster & Optik] */
win_w     = 128.0;
win_h     = 31.5;
win_r     = 1.5;
win_ch    = 1.5;          // 45°-Fase am Fenster
dif_t     = 0.8;          // Streuscheibe (0 = ohne)
deco_ring = true;         // umlaufende Zierrille um das Fenster
label     = "DOT·UHR";    // Gravur unter dem Fenster ("" = keine)
label_font = "Liberation Sans:style=Bold";
ribs      = true;         // Retro-Rillen oben und seitlich
feet      = true;         // Kufen unten

/* [ESP32-C3 Super Mini] */
esp_w     = 18.0;         // Platinenbreite
esp_l     = 22.5;         // Platinenlänge
esp_pcb_t = 1.2;          // Platinenstärke (nachmessen, 1.0–1.6)
usb_z     = 0;            // Höhe der USB-C-Mitte (0 = mittig)
usb_ovh   = 0.6;          // wie weit die USB-Buchse über die Platine ragt
usb_hole  = [9.6, 3.9];   // Durchbruch für die Buchse
usb_plug  = [14, 8.5];    // Tasche für den Stecker-Griff
usb_wall  = 0.6;          // Restwand am Durchbruch

/* [Touch TTP223 + Buzzer] */
touch     = true;         // Touch-Mulde oben + Halter innen
touch_x   = 55;           // Position (von vorne gesehen rechts)
touch_w   = 14.4;         // Platine 14 x 14 mm (+ Spiel); Winkel-Stiftleiste zeigt nach links/innen
touch_l   = 14.4;
touch_wall = 2.0;         // Restwand über dem Sensor (Modul schafft ca. 5 mm)
touch_pcb = 1.6;          // Platinenstärke TTP223 (nachmessen)
buzzer    = true;         // Halter im Deckel + Schalllöcher
buz_d     = 12.0;         // Buzzer-Durchmesser (Standard 12 mm)
buz_z     = 16;           // Höhe über Mitte

/* [Schrauben] */
boss_d   = 8;
screw_core = 2.6;         // M3 selbstschneidend (oder 4.0 für Gewindeeinsätze)
screw_hole = 3.4;
screw_head = 6.4;

$fn = 48;

// ---------------------------------------------------------------- abgeleitet
Wi  = W - 2*t;  Hi = H - 2*t;  Ri = max(R - t, 1);
Db  = D - back_t;                         // Tiefe Gehäuse ohne Deckel
pk_w = mod_w + tol;  pk_h = mod_h + tol;  // Tasche für Modul / Streuscheibe
stack = dif_t;
mod_front_y = tf + stack;
mod_back_y  = mod_front_y + mod_pcb_back; // hier drücken die Stützen des Deckels
bx = Wi/2 - boss_d/2;  bz = Hi/2 - boss_d/2;
usb_shell_h = 3.26;
esp_pcb_top = usb_z - usb_shell_h/2;

// ---------------------------------------------------------------- Helfer
function post_x() = [for (d = post_from_right) mod_w/2 - d];
module rr(w, h, r) { offset(r) square([w - 2*r, h - 2*r], center = true); }
// 2D-Profil in der XZ-Ebene, extrudiert von y0 über len in +Y
module xz(y0, len) { translate([0, y0, 0]) rotate([-90, 0, 0]) linear_extrude(len) mirror([0, 1]) children(); }

// ---------------------------------------------------------------- Gehäuse
module body() {
  difference() {
    union() {
      hull() {
        xz(0, 0.01) rr(W - 2*ch, H - 2*ch, R - ch);
        xz(ch, Db - ch) rr(W, H, R);
      }
      if (feet) for (s = [-1, 1]) skid(s * 55);
    }
    // Innenraum
    xz(tf, Db) rr(Wi, Hi, Ri);
    // Fenster mit Fase
    hull() {
      xz(-0.01, 0.01) rr(win_w + 2*win_ch, win_h + 2*win_ch, win_r + win_ch);
      xz(win_ch, 0.01) rr(win_w, win_h, win_r);
    }
    xz(-1, tf + 2) rr(win_w, win_h, win_r);
    // Zierrille
    if (deco_ring) xz(-1, 1.6) difference() {
      rr(win_w + 2*win_ch + 11, win_h + 2*win_ch + 11, 5);
      rr(win_w + 2*win_ch + 9,  win_h + 2*win_ch + 9, 4);
    }
    // Gravur
    if (label != "")
      translate([0, 0.6, -(win_h/2 + win_ch + 5.5 + 5)])
        rotate([90, 0, 0]) linear_extrude(1)
          text(label, size = 4.5, font = label_font, halign = "center", valign = "center", spacing = 1.25);
    // Retro-Rillen
    if (ribs) {
      for (x = [-36 : 6 : 36]) translate([x - 0.7, 6, H/2 - 1]) cube([1.4, Db - 12, 2]);
      for (s = [-1, 1], z = [-8, 0, 8]) translate([s * (W/2 - 1) - 1, 6, z - 0.7]) cube([2, Db - 12, 1.4]);
    }
    // Touch-Mulde oben (Restwand touch_wall)
    if (touch) translate([touch_x, Db/2, H/2 - (t - touch_wall)]) cylinder(d = 16, h = t + 1);
    // Schraublöcher in den Domen
    for (sx = [-1, 1], sz = [-1, 1])
      translate([sx * bx, Db - 14, sz * bz]) rotate([-90, 0, 0]) cylinder(d = screw_core, h = 15);
  }
  // Tasche für Streuscheibe / Modul
  difference() {
    xz(tf - 0.01, stack + 6) rr(pk_w + 4, pk_h + 4, 1);
    xz(tf - 1, stack + 8) square([pk_w, pk_h], center = true);
    // Kabelschlitze links und rechts, mittig, bis zur Frontwand (für Dupont-Stecker)
    if (cable_slot > 0) for (s = [-1, 1])
      translate([s * (pk_w/2 + 1) - 2.5, tf, -cable_slot/2]) cube([5, stack + 7, cable_slot]);
  }
  // Halter für TTP223 unter der Mulde (Einschub-Schiene)
  if (touch) touch_holder();
  // Schraubdome
  for (sx = [-1, 1], sz = [-1, 1])
    translate([sx * bx, tf - 0.01, sz * bz]) difference() {
      rotate([-90, 0, 0]) cylinder(d = boss_d, h = Db - tf);
      translate([0, Db - tf - 14, 0]) rotate([-90, 0, 0]) cylinder(d = screw_core, h = 15);
    }
}

module skid(x) {
  translate([x, 0, 0]) xz(3, Db - 6)
    polygon([[-7, -H/2 + 2], [7, -H/2 + 2], [4.5, -H/2 - 3], [-4.5, -H/2 - 3]]);
}

// ---------------------------------------------------------------- Rückdeckel
module cover() {
  difference() {
    union() {
      hull() {
        xz(Db, back_t - ch) rr(W, H, R);
        xz(D - 0.01, 0.01) rr(W - 2*ch, H - 2*ch, R - ch);
      }
      // Zentrierlippe (zwischen den Domen)
      lip = 1.6; g = 0.3;
      for (s = [-1, 1]) {
        translate([-62, Db - 3, s > 0 ? Hi/2 - g - lip : -Hi/2 + g]) cube([124, 3.01, lip]);
        translate([s > 0 ? Wi/2 - g - lip : -Wi/2 + g, Db - 3, -16]) cube([lip, 3.01, 32]);
      }
      // Andruck-Stützen für das Display-Modul (2 Stück, mittig in der Höhe)
      for (px = post_x())
        translate([px, mod_back_y + post_short, 0])
          rotate([-90, 0, 0]) difference() {
            cylinder(d = 7, h = Db - mod_back_y - post_short + 0.01);
            translate([0, 0, -0.01]) cylinder(d = 3.5, h = Db);   // leicht federnd
          }
      // Buzzer-Halter (Ring, Buzzer einkleben)
      if (buzzer) translate([0, Db - 7, buz_z]) rotate([-90, 0, 0]) difference() {
        cylinder(d = buz_d + 3.2, h = 7.01);
        translate([0, 0, -1]) cylinder(d = buz_d + 0.4, h = 9);
      }
      // ESP32-C3 Halter
      esp_holder();
    }
    // USB-C Durchbruch + Stecker-Tasche
    translate([0, 0, usb_z]) xz(Db - 1, back_t + 2) rr(usb_hole[0], usb_hole[1], 1.5);
    translate([0, 0, usb_z]) xz(Db + usb_wall, back_t) rr(usb_plug[0], usb_plug[1], 3);
    // Schrauben mit Senkkopf
    for (sx = [-1, 1], sz = [-1, 1]) translate([sx * bx, 0, sz * bz]) {
      xz(Db - 1, back_t + 2) circle(d = screw_hole);
      hull() { xz(D - 1.6, 0.01) circle(d = screw_hole); xz(D - 0.01, 0.1) circle(d = screw_head); }
    }
    // Schalllöcher für den Buzzer
    if (buzzer) for (p = [[0, 0], [3, 0], [-3, 0], [0, 3], [0, -3]])
      translate([p[0], 0, buz_z + p[1]]) xz(Db - 1, back_t + 2) circle(d = 1.8, $fn = 16);
    // Lüftungsschlitze (ausgespart, wo die Stützen sitzen)
    for (s = [-1, 1], x = [22 : 5 : 52])
      if (min([for (px = post_x()) abs(s * x - px)]) > 5)
        translate([s * x, 0, 0]) xz(Db - 1, back_t + 2) rr(2, 20, 0.9);
    // Beschriftung hinten
    translate([0, D + 0.5, usb_z + usb_plug[1]/2 + 4]) rotate([90, 0, 0]) linear_extrude(1)
      mirror([1, 0]) text("USB-C  5V", size = 3.5, font = label_font, halign = "center", valign = "center");
  }
}

// ---------------------------------------------------------------- ESP32-C3 Halter
// Platine liegt auf einer Auflage, die USB-Buchse steckt im Deckel-Durchbruch.
//  - Anschlag am hinteren Platinenende: fängt den Druck beim Kabel-Einstecken ab
//  - 2 federnde Seitenwangen mit Rastnasen: halten die Platine nach oben fest
// Einsetzen: Buchse in den Durchbruch, Platine flach nach unten drücken -> klickt ein.
esp_fin_l = 17;           // Länge der federnden Wangen
esp_snap  = 0.8;          // Überstand der Rastnasen über die Platinenkante
module esp_holder() {
  esp_bot = esp_pcb_top - esp_pcb_t;
  y_end   = Db - esp_l;                       // hinteres Platinenende
  // Auflage über die ganze Länge (zwischen den Stiftreihen)
  translate([-4.5, y_end - 1.8, esp_bot - 5]) cube([9, esp_l + 1.81, 5]);
  // Anschlag
  translate([-4.5, y_end - 1.8, esp_bot - 5]) cube([9, 1.6, 5 + esp_pcb_t + 1.5]);
  // Federnde Wangen mit Rastnase
  for (s = [-1, 1]) mirror([s < 0 ? 1 : 0, 0, 0]) {
    translate([esp_w/2 + 0.25, Db - esp_fin_l, esp_bot - 5])
      cube([1.6, esp_fin_l + 0.01, 5 + esp_pcb_t + 1.6]);
    // Rastnase mit Einführschräge (oben)
    translate([0, Db - esp_fin_l, 0]) rotate([-90, 0, 0]) linear_extrude(3)
      polygon([[esp_w/2 + 0.26, -(esp_pcb_top + 0.15)],
               [esp_w/2 + 0.26 - esp_snap, -(esp_pcb_top + 0.15)],
               [esp_w/2 + 0.26, -(esp_pcb_top + 0.15 + esp_snap + 0.8)]]);
  }
}

// ---------------------------------------------------------------- TTP223 Halter
// Schiene unter der Touch-Mulde: Modul von innen (links) einschieben,
// glatte Seite nach oben an die Wand, Bauteile + Stiftleiste nach unten.
module touch_holder() {
  zt  = Hi/2;                           // Innenseite Deckenwand
  z1  = zt - touch_pcb - 0.3;           // Unterkante Platine
  lip = 0.8;                            // Auflage unter der Platinenkante
  h   = zt - (z1 - 1.2);                // Höhe der Schienen
  translate([touch_x, Db/2, 0]) {
    // Schiene hinten (+y)
    xz_rail(touch_w + 1.2) polygon([[touch_l/2, zt + 0.01], [touch_l/2, z1], [touch_l/2 - lip, z1],
                                    [touch_l/2, z1 - lip], [touch_l/2, z1 - 1.2],
                                    [touch_l/2 + 1.4, z1 - 1.2], [touch_l/2 + 1.4, zt + 0.01]]);
    // Schiene vorne (-y), unten 45° abgeschrägt (druckbar ohne Stütze)
    xz_rail(touch_w + 1.2) polygon([[-touch_l/2, zt + 0.01], [-touch_l/2, z1], [-touch_l/2 + lip, z1],
                                    [-touch_l/2 + lip, z1 - 0.6], [-touch_l/2, z1 - 1.2],
                                    [-touch_l/2 - 1.4, z1 - 1.2], [-touch_l/2 - 1.4 - h, zt + 0.01]]);
    // Endanschlag rechts
    translate([touch_w/2, -touch_l/2 - 1.4, z1 - 1.2]) cube([1.2, touch_l + 2.8, h + 0.01]);
  }
}
// 2D-Profil in der YZ-Ebene (Punkte [y, z]), extrudiert entlang X ab -touch_w/2
module xz_rail(len) {
  translate([-touch_w/2, 0, 0]) rotate([90, 0, 90]) linear_extrude(len) children();
}

// ---------------------------------------------------------------- Streuscheibe
module diffuser() {
  translate([-(pk_w - tol)/2, 0, -(pk_h - tol)/2]) cube([pk_w - tol, dif_t, pk_h - tol]);
}

// ---------------------------------------------------------------- Dummies
module dummy_display() {
  color("#222") translate([-mod_w/2, mod_front_y, -mod_h/2]) cube([mod_w, 8, mod_h]);
  color("#1b5e20") translate([-mod_w/2, mod_front_y + 8, -mod_h/2]) cube([mod_w, mod_pcb_back - 8, mod_h]);
  color("#ff3b1f") for (i = [0 : mod_cols - 1], j = [0 : 7])
    translate([(i - (mod_cols - 1)/2) * pitch, mod_front_y - 0.2, (j - 3.5) * pitch])
      rotate([-90, 0, 0]) cylinder(d = 3, h = 0.3, $fn = 12);
}
module dummy_extra() {
  if (touch) color("#1565c0") translate([touch_x - touch_w/2 + 0.2, Db/2 - touch_l/2 + 0.2, Hi/2 - touch_pcb]) cube([touch_w - 0.4, touch_l - 0.4, touch_pcb]);
  if (buzzer) color("#111") translate([0, Db - 9.5, buz_z]) rotate([-90, 0, 0]) cylinder(d = buz_d, h = 9.5);
}
module dummy_esp() {
  color("#0d47a1") translate([-esp_w/2, Db - esp_l, esp_pcb_top - esp_pcb_t]) cube([esp_w, esp_l, esp_pcb_t]);
  color("silver") translate([-4.47, Db - 7.35 + usb_ovh, esp_pcb_top]) cube([8.94, 7.35, usb_shell_h]);
}

// ---------------------------------------------------------------- Ausgabe
// Druckausrichtung: Gehäuse mit der Front nach unten, Deckel mit der Außenseite nach unten
module body_print()  { rotate([90, 0, 0]) body(); }
module cover_print() { translate([0, 0, D]) rotate([-90, 0, 0]) cover(); }
module dif_print()   { rotate([90, 0, 0]) diffuser(); }

if (part == "body")          body_print();
else if (part == "cover")    cover_print();
else if (part == "diffuser") dif_print();
else if (part == "print_all") {
  body_print();
  translate([0, H + 15, 0]) cover_print();
  translate([0, -H/2 - 30, 0]) dif_print();
}
else {   // assembly
  color("#c8b38a") body();
  color("#8a6a45") cover();
  dummy_display();
  dummy_esp();
  dummy_extra();
}
