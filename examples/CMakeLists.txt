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
    DESTINATION examples
    PATTERN "node_modules" EXCLUDE
)
