// Hello World cart — centered text example

function _init() {
    gfx.cls(0);
}

function _update(dt) {
    // Update logic here
}

function _draw() {
    gfx.cls(0);
    gfx.print("Hello World", 133, 87, 7);

    // FPS counter
    var fps_str = "FPS:" + math.flr(sys.fps());
    var fps_w = fps_str.length * 6 + 2;
    gfx.rectfill(320 - fps_w, 0, 319, 8, 0);
    gfx.print(fps_str, 321 - fps_w, 1, 7);
}
