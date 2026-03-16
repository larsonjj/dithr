// Sprite & Animation demo
//
// Shows how to:
//   - Draw sprites with gfx.spr()
//   - Animate by cycling sprite indices
//   - Flip sprites horizontally/vertically
//   - Rotate sprites with gfx.spr_rot()
//
// Replace the placeholder rectangles with a real spritesheet to see
// actual sprite art.  The animation logic stays the same.

// --- State -----------------------------------------------------------

var frame = 0; // animation frame counter
var anim_t = 0; // accumulator for animation timing
var anim_speed = 0.12; // seconds per frame

// PLACEHOLDER: sprite indices for a 4-frame walk cycle
// With a real sheet, set these to the actual tile indices.
var walk_frames = [0, 1, 2, 3];
var current_frame = 0;

var facing_right = true;
var rotation = 0;

// --- Callbacks -------------------------------------------------------

function _init() {
    gfx.cls(0);
}

function _update(dt) {
    ++frame;

    // Advance animation timer
    anim_t += dt;
    if (anim_t >= anim_speed) {
        anim_t -= anim_speed;
        current_frame = (current_frame + 1) % walk_frames.length;
    }

    // Toggle direction every 2 seconds
    if (frame % 120 === 0) {
        facing_right = !facing_right;
    }

    // Spin the rotation demo
    rotation += dt * 2.0;
}

function _draw() {
    gfx.cls(1);

    // --- Section 1: Basic sprite drawing ---
    gfx.color(7);
    gfx.print("gfx.spr() basics", 4, 4);

    // Draw the first 4 sprites in a row (or placeholders)
    for (var i = 0; i < 4; ++i) {
        gfx.spr(i, 8 + i * 16, 16);
    }

    // --- Section 2: Animated walk cycle ---
    gfx.print("animation cycle", 4, 36);

    // Draw the current walk frame
    var spr_idx = walk_frames[current_frame];
    gfx.spr(spr_idx, 8, 48);

    // Label the frame number
    gfx.print("frame " + current_frame, 24, 50, 6);

    // --- Section 3: Horizontal / vertical flip ---
    gfx.print("flip_x / flip_y", 4, 68);

    gfx.spr(0, 8, 80, 1, 1, false, false); // normal
    gfx.spr(0, 24, 80, 1, 1, true, false); // flip X
    gfx.spr(0, 40, 80, 1, 1, false, true); // flip Y
    gfx.spr(0, 56, 80, 1, 1, true, true); // flip both

    gfx.print("N", 10, 90, 6);
    gfx.print("X", 26, 90, 6);
    gfx.print("Y", 42, 90, 6);
    gfx.print("XY", 56, 90, 6);

    // --- Section 4: Multi-tile sprites ---
    gfx.print("multi-tile (2x2)", 4, 104);

    // Draw a 2x2 tile sprite starting at index 0
    gfx.spr(0, 8, 116, 2, 2);

    // --- Section 5: Rotation ---
    gfx.print("spr_rot()", 4, 140);

    // Rotate sprite 0 around its center
    gfx.spr_rot(0, 8, 154, rotation, -1, -1);

    gfx.print("angle: " + math.flr(rotation * 100) / 100, 24, 156, 6);

    // --- Section 6: Stretch blit ---
    gfx.print("sspr() stretch", 140, 4);

    // Stretch sprite region (0,0,8,8) to a 32x32 destination
    gfx.sspr(0, 0, 8, 8, 144, 16, 32, 32);

    // --- Info ---
    gfx.print("replace spritesheet.png", 140, 60, 5);
    gfx.print("to see real art!", 140, 68, 5);
}
