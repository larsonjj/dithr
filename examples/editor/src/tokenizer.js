// ─── Syntax highlighting ─────────────────────────────────────────────────────

import { COMCOL, STRCOL, NUMCOL, FG, KWCOL, KEYWORDS } from "./config.js";

export function tokenize(line, inBlock) {
    let toks = [];
    let i = 0,
        n = line.length;

    if (inBlock) {
        let end = line.indexOf("*/");
        if (end >= 0) {
            toks.push({ text: line.slice(0, end + 2), col: COMCOL });
            i = end + 2;
            inBlock = false;
        } else {
            toks.push({ text: line, col: COMCOL });
            return { toks: toks, inBlock: true };
        }
    }

    while (i < n) {
        if (line[i] === "/" && i + 1 < n && line[i + 1] === "*") {
            let end = line.indexOf("*/", i + 2);
            if (end >= 0) {
                toks.push({ text: line.slice(i, end + 2), col: COMCOL });
                i = end + 2;
                continue;
            } else {
                toks.push({ text: line.slice(i), col: COMCOL });
                return { toks: toks, inBlock: true };
            }
        }

        if (line[i] === "/" && i + 1 < n && line[i + 1] === "/") {
            toks.push({ text: line.slice(i), col: COMCOL });
            return { toks: toks, inBlock: false };
        }

        if (line[i] === '"' || line[i] === "'" || line[i] === "`") {
            let q = line[i],
                j = i + 1;
            while (j < n && line[j] !== q) {
                if (line[j] === "\\") j++;
                j++;
            }
            if (j < n) j++;
            toks.push({ text: line.slice(i, j), col: STRCOL });
            i = j;
            continue;
        }

        if (line[i] >= "0" && line[i] <= "9") {
            let j = i;
            while (
                j < n &&
                ((line[j] >= "0" && line[j] <= "9") ||
                    line[j] === "." ||
                    line[j] === "x" ||
                    line[j] === "X" ||
                    (line[j] >= "a" && line[j] <= "f") ||
                    (line[j] >= "A" && line[j] <= "F"))
            )
                j++;
            toks.push({ text: line.slice(i, j), col: NUMCOL });
            i = j;
            continue;
        }

        let ch = line[i];
        if ((ch >= "a" && ch <= "z") || (ch >= "A" && ch <= "Z") || ch === "_" || ch === "$") {
            let j = i + 1;
            while (j < n) {
                let c = line[j];
                if (
                    (c >= "a" && c <= "z") ||
                    (c >= "A" && c <= "Z") ||
                    (c >= "0" && c <= "9") ||
                    c === "_" ||
                    c === "$"
                )
                    j++;
                else break;
            }
            let word = line.slice(i, j);
            toks.push({ text: word, col: KEYWORDS.has(word) ? KWCOL : FG });
            i = j;
            continue;
        }

        toks.push({ text: line[i], col: FG });
        i++;
    }
    return { toks: toks, inBlock: false };
}
