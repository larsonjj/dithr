# @dithrkit/create

Scaffold a new [dithr](https://github.com/dithrkit/dithr) project.

## Usage

```bash
npm create @dithrkit my-game
```

### Options

```
--template, -t <name>  Template to use (blank, platformer)
--typescript, --ts     Scaffold a TypeScript project
```

### Examples

```bash
# JavaScript project with the blank template
npm create @dithrkit my-game

# TypeScript project with the platformer template
npm create @dithrkit my-game --ts --template platformer
```

## What gets created

```
my-game/
├── cart.json
├── package.json
├── eslint.config.js
├── .prettierrc
├── .editorconfig
├── .gitignore
├── README.md
├── src/
│   └── main.js (or main.ts)
└── assets/
    ├── sprites/
    ├── maps/
    ├── sfx/
    └── music/
```

## License

MIT
