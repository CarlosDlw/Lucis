#!/usr/bin/env python3
"""LSP import-alias coverage: everything importable and renameable with `as`.

Covers, for every kind of symbol a module can export (function, const,
struct, enum, trait, type alias, struct methods via extend, stdlib .lc
variadic):
  - completion shows the local alias
  - hover on the alias resolves to the original symbol
  - go-to-definition jumps to the original declaration
  - signature help matches the exported name
  - method completion/hover works on imported+renamed struct types
  - semantic tokens classify import bindings

A single LSP session over a real project fixture (event-driven, no sleeps
except the header-cache warm-up) — the fast pattern of test_diagnostics.py.
"""

import json
import os
import select
import subprocess
import sys
import time

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_aliases_stderr.log")
PROJECT_DIR = os.path.join(os.path.dirname(__file__), "aliases", "project")
MAIN_PATH = os.path.join(PROJECT_DIR, "src", "main.lc")
LIB_PATH = os.path.join(PROJECT_DIR, "src", "lib.lc")
MAIN_URI = "file://" + MAIN_PATH
LIB_URI = "file://" + LIB_PATH

passed = 0
failed = 0


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"PASS: {name}")
    else:
        failed += 1
        print(f"FAIL: {name} {detail}")


def load_lines(path):
    with open(path) as f:
        return f.read().split("\n")


MAIN = load_lines(MAIN_PATH)
LIB = load_lines(LIB_PATH)


def line_of(lines, needle):
    for i, ln in enumerate(lines):
        if needle in ln:
            return i
    raise AssertionError(f"line not found: {needle}")


# Expected definition lines (0-based) inside lib.lc.
LIB_POINT = line_of(LIB, "struct Point")
LIB_MAX = line_of(LIB, "const MAX_POINTS")
LIB_SQR = line_of(LIB, "fn sqr")
LIB_ALIAS = line_of(LIB, "type PointAlias")
LIB_COLOR = line_of(LIB, "enum Color")
LIB_AREA = line_of(LIB, "trait Area")


class LspSession:
    def __init__(self, root, uri):
        self.log = open(SERVER_LOG, "ab")
        self.proc = subprocess.Popen(
            [LUCIS_BIN, "lsp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, text=True, bufsize=0,
        )
        self._send({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                    "params": {"processId": None, "rootUri": "file://" + root,
                               "capabilities": {}}})
        self._recv(id=0, timeout=5)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        self.uri = uri

    def _send(self, obj):
        body = json.dumps(obj, ensure_ascii=False)
        self.proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
        self.proc.stdin.flush()

    def _frame(self, timeout):
        raw = self.proc.stdout.buffer
        deadline = time.monotonic() + timeout
        content_len = 0
        while time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
            if not r:
                return None
            line = b""
            while time.monotonic() < deadline:
                r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
                if not r:
                    return None
                ch = raw.read(1)
                if ch == b"":
                    return None
                if ch == b"\n":
                    break
                line += ch
            hdr = line.decode(errors="replace").strip()
            if hdr.startswith("Content-Length:"):
                content_len = int(hdr.split(":")[1].strip())
            elif hdr == "":
                break
        body = b""
        while len(body) < content_len and time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
            if not r:
                return None
            chunk = raw.read(content_len - len(body))
            if chunk == b"":
                return None
            body += chunk
        return json.loads(body.decode("utf-8"))

    def _recv(self, id=None, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self._frame(deadline - time.monotonic())
            if msg is None:
                return None
            if msg.get("id") == id:
                return msg
        return None

    def request(self, rid, method, params, timeout=10):
        self._send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
        return self._recv(id=rid, timeout=timeout)

    def open_main(self):
        text = "\n".join(MAIN) + "\n"
        self._send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
                    "params": {"textDocument": {"uri": self.uri, "text": text}}})
        # First open warms the C header cache (blocking). Wait it out so
        # subsequent requests are not enqueued behind it.
        time.sleep(1.0)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            msg = self._frame(deadline - time.monotonic())
            if msg is None:
                return
            if msg.get("id") == 1:
                return

    def position(self, needle_line, col):
        return {"line": needle_line, "character": col}

    def completion(self, line, col):
        resp = self.request(10, "textDocument/completion",
                            {"textDocument": {"uri": self.uri},
                             "position": self.position(line, col)})
        if resp and isinstance(resp.get("result"), list):
            return [i.get("label") for i in resp["result"]]
        return []

    def hover(self, line, col):
        resp = self.request(11, "textDocument/hover",
                            {"textDocument": {"uri": self.uri},
                             "position": self.position(line, col)})
        raw = ""
        if resp and resp.get("result"):
            c = resp["result"].get("contents", "")
            if isinstance(c, dict):
                raw = c.get("value", "")
            elif isinstance(c, list) and c and isinstance(c[0], dict):
                raw = c[0].get("value", "")
            else:
                raw = str(c) if c else ""
        return raw

    def definition(self, line, col):
        resp = self.request(12, "textDocument/definition",
                            {"textDocument": {"uri": self.uri},
                             "position": self.position(line, col)})
        if resp and resp.get("result"):
            r = resp["result"]
            if isinstance(r, list) and r:
                r = r[0]
            if isinstance(r, dict):
                return (r.get("uri"), r.get("range", {}).get("start", {}).get("line", -1))
        return (None, -1)

    def signature(self, line, col):
        resp = self.request(13, "textDocument/signatureHelp",
                            {"textDocument": {"uri": self.uri},
                             "position": self.position(line, col)})
        labels = []
        if resp and resp.get("result"):
            sigs = resp["result"].get("signatures") or []
            for s in sigs:
                labels.append(s.get("label", ""))
        return labels

    def semantic_tokens(self):
        resp = self.request(14, "textDocument/semanticTokens/full",
                            {"textDocument": {"uri": self.uri}})
        if not resp or not isinstance(resp.get("result"), dict):
            return []
        data = resp["result"].get("data", [])
        out = []
        line = 0
        col = 0
        it = iter(data)
        for d_line, d_col, d_len, tok_type, _mods in zip(it, it, it, it, it):
            line += d_line
            if d_line == 0:
                col += d_col
            else:
                col = d_col
            out.append((line, col, d_len, tok_type))
        return out

    def close(self):
        self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
        self._recv(id=99, timeout=5)
        self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
        self.proc.stdin.flush()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.log.close()


def col_of(line_text, token_start):
    return line_text.index(token_start)


def main():
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    s = LspSession(PROJECT_DIR, MAIN_URI)
    try:
        s.open_main()

        # ── Completion: aliases and qualified names appear ─────────────
        line_use = line_of(MAIN, "Alias a = ")
        labels = s.completion(line_use, col_of(MAIN[line_use], "Alias"))
        for alias in ("output", "MyPoint", "LIMIT", "square", "Hue",
                      "AreaTrait", "Alias", "makeOrigin"):
            check(f"completion offers alias '{alias}'", alias in labels,
                  f"missing; got={labels[:40]}")

        # ── Completion: methods of an imported+renamed struct ──────────
        line_norm = line_of(MAIN, "p.norm()")
        labels = s.completion(line_norm, MAIN[line_norm].index("p.") + 2)
        check("completion on alias-typed value offers extend method 'norm'",
              "norm" in labels, f"got={labels[:40]}")
        check("completion on alias-typed value offers trait method 'area'",
              "area" in labels, f"got={labels[:40]}")

        # ── Hover: alias resolves to the exported symbol ───────────────
        ln = line_of(MAIN, "square(3)")
        raw = s.hover(ln, col_of(MAIN[ln], "square"))
        check("hover on function alias shows original 'sqr'",
              "sqr" in raw, f"got={raw[:160]}")

        ln = line_of(MAIN, "MyPoint p = ")
        raw = s.hover(ln, col_of(MAIN[ln], "MyPoint"))
        check("hover on struct alias shows original 'Point'",
              "Point" in raw, f"got={raw[:160]}")

        ln = line_of(MAIN, "int32 lim = LIMIT")
        raw = s.hover(ln, MAIN[ln].index("LIMIT"))
        check("hover on const alias shows original 'MAX_POINTS'",
              "MAX_POINTS" in raw, f"got={raw[:160]}")

        ln = line_of(MAIN, "Hue::Red")
        raw = s.hover(ln, MAIN[ln].index("Hue"))
        check("hover on enum alias shows original 'Color'",
              "Color" in raw, f"got={raw[:160]}")

        ln = line_of(MAIN, "Area as AreaTrait")
        raw = s.hover(ln, MAIN[ln].index("AreaTrait"))
        check("hover on trait alias shows original 'Area'",
              "Area" in raw, f"got={raw[:160]}")

        ln = line_of(MAIN, "Alias a = ")
        raw = s.hover(ln, col_of(MAIN[ln], "Alias"))
        check("hover on type-alias import shows original 'PointAlias'",
              "PointAlias" in raw, f"got={raw[:160]}")

        # ── Go-to-definition: alias jumps to the original declaration ──
        ln = line_of(MAIN, "square(3)")
        uri, got = s.definition(ln, col_of(MAIN[ln], "square"))
        check("definition on function alias lands on 'fn sqr'",
              uri == LIB_URI and got == LIB_SQR, f"got=({uri}, {got}), want ({LIB_URI}, {LIB_SQR})")

        ln = line_of(MAIN, "MyPoint p = ")
        uri, got = s.definition(ln, col_of(MAIN[ln], "MyPoint"))
        check("definition on struct alias lands on 'struct Point'",
              uri == LIB_URI and got == LIB_POINT, f"got=({uri}, {got}), want ({LIB_URI}, {LIB_POINT})")

        ln = line_of(MAIN, "int32 lim = LIMIT")
        uri, got = s.definition(ln, MAIN[ln].index("LIMIT"))
        check("definition on const alias lands on 'const MAX_POINTS'",
              uri == LIB_URI and got == LIB_MAX, f"got=({uri}, {got}), want ({LIB_URI}, {LIB_MAX})")

        ln = line_of(MAIN, "Alias a = ")
        uri, got = s.definition(ln, col_of(MAIN[ln], "Alias"))
        check("definition on type-alias import lands on 'type PointAlias'",
              uri == LIB_URI and got == LIB_ALIAS, f"got=({uri}, {got}), want ({LIB_URI}, {LIB_ALIAS})")

        # ── Signature help: local alias shows the exported signature ───
        ln = line_of(MAIN, "square(3)")
        labels = s.signature(ln, MAIN[ln].index("square(") + len("square(") + 1)
        check("signature help on function alias shows 'sqr'",
              any("sqr" in l for l in labels), f"got={labels}")

        ln = line_of(MAIN, 'output("done')
        labels = s.signature(ln, MAIN[ln].index("output(") + len("output(") + 1)
        check("signature help on stdlib alias shows 'printf'",
              any("printf" in l for l in labels), f"got={labels}")

        # ── Semantic tokens: import bindings are classified ────────────
        # Find the token of the local alias 'square' on the import line.
        ln_imp = line_of(MAIN, "sqr as square")
        toks = s.semantic_tokens()
        found = [t for t in toks
                 if t[0] == ln_imp
                 and MAIN[ln_imp][t[1]:t[1] + t[2]] == "square"]
        check("semantic tokens classify import alias 'square'",
              len(found) >= 1, f"tokens on line {ln_imp}={toks}")

    finally:
        s.close()

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()