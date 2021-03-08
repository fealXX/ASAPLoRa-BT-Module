$fn = 50;

facet = 1;

wall_thcknss = 2;

pcb_width = 42;
pcb_height = 105;
pcb_clearance_chips = 18;
pcb_thickness = 2;
m3_insert_hole = 4;
m3_insert_height = 4;
pcb_stud_clear = 2;

power_led_width = 6;
power_led_height = 6;

power_switch_width = 11.5;
power_switch_height = 6;

module outer_shell_body(){
    difference(){
        minkowski(){
            cube([
            pcb_width + 2*wall_thcknss - facet, pcb_height + 2*wall_thcknss - facet, 
            pcb_clearance_chips + wall_thcknss - facet
            ], center=true);
            sphere(facet);
        }
        
        translate([0,0,pcb_clearance_chips + wall_thcknss])
        cube([pcb_width + 2*wall_thcknss + 10, pcb_height + 2*wall_thcknss + 10, pcb_clearance_chips + wall_thcknss], center=true);
        
        
        translate([0,0,wall_thcknss])
        minkowski(){
            cube([pcb_width, pcb_height, pcb_clearance_chips], center=true);
            sphere(facet);
        }
    }
}

module power_led_cutout(){
    translate([
        0,
        -pcb_height / 2 - wall_thcknss,
        pcb_clearance_chips / 2 - power_led_height / 2 + facet - pcb_thickness
    ])
    minkowski(){
        cube([power_led_width - 2*facet, wall_thcknss, power_led_height - 2*facet], center=true);
        sphere(facet);
    }
}

module power_switch_cutout(){
    translate([
        0,
        pcb_height / 2 + wall_thcknss,
        pcb_clearance_chips / 2 - power_switch_height / 2 + facet - pcb_thickness
    ])
    cube([power_switch_width, 2*wall_thcknss, power_switch_height], center=true);
    
}

module lora_antenna(){
    translate([
        4.4,
        pcb_height / 2 - 8,
        -pcb_clearance_chips
    ])
        cylinder(wall_thcknss + pcb_clearance_chips, d=11);
    
}


module outer_shell(){
    difference(){
        outer_shell_body();
        power_led_cutout();
        power_switch_cutout();
        lora_antenna();
    }
}

module pcb_stud(){
    difference(){
        translate([
            m3_insert_hole / 2 + pcb_stud_clear - wall_thcknss/4 - facet, 
            m3_insert_hole / 2 + pcb_stud_clear - wall_thcknss/4 - facet,
            wall_thcknss
        ])
            linear_extrude(pcb_clearance_chips)
                minkowski(){
                    square([
                        2 * m3_insert_hole + wall_thcknss / 2 - 2*facet, 
                        2 * m3_insert_hole + wall_thcknss / 2 - 2*facet
                    ], center=true);
                    circle(facet);
                }

        translate([
                m3_insert_hole / 2 + pcb_stud_clear, 
                m3_insert_hole / 2 + pcb_stud_clear,
                1
        ])
            cylinder(pcb_clearance_chips + wall_thcknss, d=m3_insert_hole);
    }
}

module pcb_studs(){
    translate([pcb_width/2, pcb_height/2, -pcb_clearance_chips/2 - wall_thcknss])
    rotate(180)
    pcb_stud();
    translate([-pcb_width/2, pcb_height/2, -pcb_clearance_chips/2 - wall_thcknss])
    rotate(270)
    pcb_stud();
    translate([pcb_width/2, -pcb_height/2, -pcb_clearance_chips/2 - wall_thcknss])
    rotate(90)
    pcb_stud();
}

union(){
    outer_shell();
    pcb_studs();
}