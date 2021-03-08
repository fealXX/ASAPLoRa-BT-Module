$fn = 50;

facet = 1;

wall_thcknss = 2;

pcb_width = 42;
pcb_height = 105;
pcb_clearance_battery = 22;
pcb_thickness = 2;
m3_head = 6.2;
m3_insert_hole = 4;
m3_screw_hole = 3.5;
m3_insert_height = 4;
pcb_stud_clear = 2;

power_plug_width = 10;
power_plug_height = 10;

module outer_shell_body(){
    difference(){
        minkowski(){
            cube([
            pcb_width + 2*wall_thcknss - facet, pcb_height + 2*wall_thcknss - facet, 
            pcb_clearance_battery + wall_thcknss - facet
            ], center=true);
            sphere(facet);
        }
        
        translate([0,0,pcb_clearance_battery + wall_thcknss])
        cube([pcb_width + 2*wall_thcknss + 10, pcb_height + 2*wall_thcknss + 10, pcb_clearance_battery + wall_thcknss], center=true);
        
        
        translate([0,0,wall_thcknss])
        minkowski(){
            cube([pcb_width, pcb_height, pcb_clearance_battery], center=true);
            sphere(facet);
        }
    }
}

module power_plug_cutout(){
    translate([
        0,
        -pcb_height / 2 - wall_thcknss,
        pcb_clearance_battery / 2 - power_plug_height / 2 + facet
    ])
    minkowski(){
        cube([power_plug_width, wall_thcknss, power_plug_height], center=true);
        sphere(facet);
    }
}


module outer_shell(){
    difference(){
        outer_shell_body();
        power_plug_cutout();
    }
}

module pcb_stud(){
    difference(){
        translate([
            m3_insert_hole / 2 + pcb_stud_clear - wall_thcknss/4 - facet, 
            m3_insert_hole / 2 + pcb_stud_clear - wall_thcknss/4 - facet,
            wall_thcknss
        ])
            linear_extrude(pcb_clearance_battery)
                minkowski(){
                    square([
                        2 * m3_insert_hole + wall_thcknss / 2 - facet, 
                        2 * m3_insert_hole + wall_thcknss / 2 - facet
                    ], center=true);
                    circle(facet);
                }

        translate([
                m3_insert_hole / 2 + pcb_stud_clear, 
                m3_insert_hole / 2 + pcb_stud_clear,
                1
        ])
            cylinder(pcb_clearance_battery + wall_thcknss, d=m3_screw_hole);
    }
}

module pcb_studs(){
    translate([pcb_width/2, pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
    rotate(180)
    pcb_stud();
    
    translate([-pcb_width/2, pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
    rotate(270)
    pcb_stud();
    
    translate([pcb_width/2, -pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
    rotate(90)
    pcb_stud();
    
    translate([-pcb_width/2, -pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
    pcb_stud();
}

module case_screw(){

    translate([
            m3_insert_hole / 2 + pcb_stud_clear, 
            m3_insert_hole / 2 + pcb_stud_clear,
            -(pcb_clearance_battery / 2)
    ])
        cylinder(pcb_clearance_battery + wall_thcknss, d=m3_head);
}

module case_screws(){
    translate([pcb_width/2, pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
        rotate(180)
            case_screw();
    
    translate([-pcb_width/2, -pcb_height/2, -pcb_clearance_battery/2 - wall_thcknss])
        case_screw();
}

difference(){
    union(){
        outer_shell();
        pcb_studs();
    }
    case_screws();
}