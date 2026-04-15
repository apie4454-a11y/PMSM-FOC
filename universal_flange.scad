// Universal Flange - 4 Hole Design
// ID: 8mm, PCD: 20mm, 4x M3 Screws
// 3D Printable Design

// ========== PARAMETERS ==========
// Core dimensions
id = 8;              // Inner diameter (mm)
pcd = 20;            // Pitch circle diameter (mm)
num_holes = 4;       // Number of mounting holes
screw_dia = 3.2;     // M3 screw hole diameter (3.2mm for clearance)

// Proportional dimensions
flange_od = pcd + 12;  // Outer diameter (proportional)
flange_thickness = 5;   // Thickness (proportional to ID)
boss_height = 10;       // Center boss height (increased for grub screw)
grub_screw_dia = 3.2;   // M3 grub screw hole diameter
grub_screw_radius = grub_screw_dia / 2;

// ========== DERIVED VALUES ==========
hole_radius = id / 2;
boss_radius = 6.5;            // Smaller boss to clear mounting holes (13mm OD)
pcd_radius = pcd / 2;
screw_radius = screw_dia / 2;

// ========== MAIN FLANGE ==========
difference() {
    // Base flange disc
    cylinder(h = flange_thickness, r = flange_od / 2, $fn = 64);
    
    // Center hole
    translate([0, 0, -0.5])
    cylinder(h = flange_thickness + 1, r = hole_radius, $fn = 32);
    
    // 4 slotted holes around PCD (for tolerance adjustment)
    // Slots are radial to allow ±1mm PCD adjustment (19-21mm range)
    for (i = [0 : num_holes - 1]) {
        angle = (360 / num_holes) * i;
        translate([
            pcd_radius * cos(angle),
            pcd_radius * sin(angle),
            -0.5
        ])
        rotate([0, 0, angle])
        // Radial slot: 2.5mm radial length (covers 19-21mm range)
        hull() {
            cylinder(h = flange_thickness + 1, r = screw_radius, $fn = 16);
            translate([2.5, 0, 0])
            cylinder(h = flange_thickness + 1, r = screw_radius, $fn = 16);
        }
    }
}

// ========== CENTER BOSS (optional strengthening) ==========
translate([0, 0, flange_thickness])
difference() {
    // Boss ring
    cylinder(h = boss_height, r = boss_radius, $fn = 32);
    
    // Hollow center for shaft passage
    translate([0, 0, -0.5])
    cylinder(h = boss_height + 1, r = hole_radius, $fn = 32);
    
    // Radial grub screw hole (M3) for shaft clamping
    // Goes through the side of boss at mid-height
    translate([0, 0, boss_height / 2])
    rotate([0, 90, 0])
    cylinder(h = boss_radius * 2 + 2, r = grub_screw_radius, center = true, $fn = 16);
}

// ========== DESIGN NOTES ==========
// - All dimensions in millimeters
// - Suitable for FDM 3D printing (layer height 0.2mm recommended)
// - Material: PLA, PETG, or ABS
// - Print orientation: flat (with flange_thickness vertical)
// - Supports: May be needed under mounting holes
// - Screw holes are M3 clearance (3.2mm) - adjust if needed for fit
//
// TO USE:
// 1. Open in OpenSCAD (free software)
// 2. Export as STL
// 3. Slice and print
//
// TO CUSTOMIZE:
// - Change flange_od for larger/smaller overall size
// - Change flange_thickness for thicker/thinner flange
// - Change screw_dia if using different screws (M2.5 = 2.7mm, M4 = 4.2mm)
// - Change PCD for different hole spacing
