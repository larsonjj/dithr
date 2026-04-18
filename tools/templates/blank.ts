// Blank cart template — minimal starting point

export function _init(): void {
    gfx.cls(0);
}

export function _update(dt: number): void {
    // Update logic here
}

export function _draw(): void {
    gfx.cls(0);
    gfx.print('hello, dithr!', 4, 4, 7);
}
