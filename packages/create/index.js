#!/usr/bin/env node
"use strict";

// Thin wrapper so `npm create @dithrkit` works.
// Delegates to `dithrkit create` from @dithrkit/sdk.

// Forward all args to the create command
process.argv = [process.argv[0], process.argv[1], ...process.argv.slice(2)];

require("@dithrkit/sdk/lib/commands/create.js");
