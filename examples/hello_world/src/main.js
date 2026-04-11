// Hello World cart — centered text example



function _init() {
    gfx.cls(0);
}

function _update(dt) {
}

function _draw() {
    gfx.cls(0);
    gfx.print('Hello World', 133, 87, 7);

    // FPS widget
    sys.drawFps();
}
