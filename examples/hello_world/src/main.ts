// Hello World cart — centered text example



export function _init(): void {
    gfx.cls(0);
}

export function _update(_dt: number): void {
}

export function _draw(): void {
    gfx.cls(0);
    gfx.print('Hello World', 133, 87, 7);

    // FPS widget
    sys.drawFps();
}
