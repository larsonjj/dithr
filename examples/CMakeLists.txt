# Example carts are JS-only and loaded at runtime by the dithr engine.
# No build targets are needed; run them with:
#   dithr <cart-folder>
# e.g.  dithr examples/hello_world
#
# Available examples:
#   hello_world        - Minimal starter cart
#   sprite_animation   - Sprite sheet & frame animation
#   input_demo         - Keyboard, gamepad & mouse input
#   tilemap            - Tile map rendering & scrolling
#   audio_demo         - SFX & music playback
#   postfx_demo        - Post-processing effect stack
#   persistence        - Save/load data with cart.dset/dget
#   platformer         - Simple side-scrolling platformer
#   raycaster          - Wolfenstein-style raycaster
#   spritemark         - Bunnymark-style sprite benchmark
#   ui_demo            - UI layout helpers & tween animations
#   editor             - Minimal text editor (640×360 framebuffer)
#   gauntlet_clone     - Top-down dungeon crawler (Gauntlet-style)
#   map_demo           - Tilemap API, camera helpers & map objects
#   event_demo         - Event bus and system events
#   transition_demo    - Screen transitions (fade, wipe, dissolve)
#   drawlist_demo      - Depth-sorted rendering with draw list
#   palette_fx         - Palette remapping, fill patterns & custom fonts
#   collision_demo     - Collision helper functions
#   sys_explorer       - System introspection, file I/O & clipboard
#   tween_showcase     - Tweening, easing & runtime input remapping
#   synth_demo         - Procedural sound synthesis with the synth API
#   touch_demo         - Multi-touch input: position, pressure, gestures
#   ui_layout_demo     - UI layout helpers: rect, split, stack, anchor, place

# Install example carts alongside the executable
install(DIRECTORY
    hello_world
    sprite_animation
    input_demo
    tilemap
    audio_demo
    postfx_demo
    persistence
    platformer
    raycaster
    spritemark
    ui_demo
    editor
    gauntlet_clone
    map_demo
    event_demo
    transition_demo
    drawlist_demo
    palette_fx
    collision_demo
    sys_explorer
    tween_showcase
    synth_demo
    DESTINATION examples
    PATTERN "node_modules" EXCLUDE
)
