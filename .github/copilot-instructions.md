Follow the project conventions described in README.md and the coding style rules in docs/code-style.md (C11) and docs/js-code-style.md (JavaScript). Also, be sure to match formatting and naming style, as the codebase is formatted with clang-format and linted with clang-tidy for C11 and ESLint + Prettier for JavaScript.

After editing any `.c` or `.h` files, always run `clang-format -i` on the changed files before finishing. CI enforces clang-format with `-Werror`, so unformatted code will fail the build.

After editing any JavaScript files in `examples/`, verify linting passes locally by running `npm ci --ignore-scripts && npm run lint` inside the affected example directory. CI runs this for every example that has a `lint` script in its `package.json`.
