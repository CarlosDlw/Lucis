"""Deep MIXED chains: dot + arrow + index + method, deeply nested, across
completion, hover and signature help. Ensures the LSP keeps resolving the
chain and never loses the receiver type."""

import json
import os
import select
import subprocess
import sys
import time

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_stderr.log")
CHAIN_DIR = os.path.join(os.path.dirname(__file__), "_chains")

SRC = (
    "struct D { int32 n; }\n"
    "struct C { D d; }\n"
    "struct B { C c; }\n"
    "struct A { B b; }\n"
    "extend D { fn run(&self, int32 v) int32 { ret v; } }\n"
    "extend B { fn mid(&self, int32 v) C { ret self->c; } }\n"
    "\n"
    "fn main() int32 {\n"
    "  A a;\n"
    "  vec<B> items;\n"
    "  a.b.mid(1).d.run(2);\n"
    "  items[0].c.d.n;\n"
    "  items[0].mid(3).d.run(4);\n"
    "  return 0;\n"
    "}\n"
)

URI = "file://" + os.path.join(CHAIN_DIR, "_mixed.lc")
ROOT = "file://" + CHAIN_DIR

passed = 0
failed = 0


class LspClient:
    def __init__(self):
        self.log = open(SERVER_LOG, "ab")
        self.proc = subprocess.Popen(
            [LUCIS_BIN, "lsp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, text=True, bufsize=0,
        )
        time.sleep(0.15)
        self._init()

    def _init(self):
        self._send({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                    "params": {"processId": None, "rootUri": ROOT,
                               "capabilities": {}}})
        self._recv(timeout=10)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    def _send(self, obj):
        body = json.dumps(obj, ensure_ascii=False)
        self.proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
        self.proc.stdin.flush()

    def _recv(self, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self._recv_one(deadline - time.monotonic())
            if msg is None:
                return None
            if "id" in msg:
                return msg
        return None

    def _recv_one(self, timeout=10):
        raw = self.proc.stdout.buffer
        deadline = time.monotonic() + timeout
        line = b""
        while time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.1, deadline - time.monotonic()))
            if r:
                ch = raw.read(1)
                if not ch:
                    return None
                line += ch
                if line.endswith(b"\r\n"):
                    break
            else:
                return None
        hdr = line.decode().strip()
        if not hdr.startswith("Content-Length:"):
            return None
        n = int(hdr.split(":")[1].strip())
        raw.read(2)
        body = b""
        while len(body) < n:
            chunk = raw.read(n - len(body))
            if not chunk:
                break
            body += chunk
        return json.loads(body)

    def _request(self, method, params, timeout=15):
        self._send({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
        while True:
            msg = self._recv(timeout=timeout)
            if msg is None:
                return None
            if msg.get("id") == 1:
                return msg

    def open_doc(self):
        self._send({"jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
                    "params": {"textDocument": {"uri": URI, "languageId": "lucis",
                                                "version": 1, "text": SRC}}})
        # drain publishDiagnostics
        self._recv(timeout=15)

    def complete(self, line, col):
        r = self._request("textDocument/completion", {
            "textDocument": {"uri": URI},
            "position": {"line": line, "character": col},
        })
        if not r or not r.get("result"):
            return []
        res = r["result"]
        items = res.get("items", res) if isinstance(res, dict) else res
        return [i.get("label", "") for i in items]

    def hover(self, line, col):
        r = self._request("textDocument/hover", {
            "textDocument": {"uri": URI},
            "position": {"line": line, "character": col},
        })
        if not r or not r.get("result"):
            return ""
        c = r["result"].get("contents", {})
        return c.get("value", "") if isinstance(c, dict) else str(c)

    def signature(self, line, col):
        r = self._request("textDocument/signatureHelp", {
            "textDocument": {"uri": URI},
            "position": {"line": line, "character": col},
        })
        if not r or not r.get("result"):
            return []
        return [s.get("label", "") for s in r["result"].get("signatures", [])]

    def close(self):
        try:
            self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
            self._recv(timeout=3)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
        except Exception:
            pass
        try:
            self.proc.wait(timeout=3)
        except Exception:
            self.proc.kill()
        self.log.close()


def check(desc, ok, detail=""):
    global passed, failed
    if ok:
        print(f"  OK   {desc}")
        passed += 1
    else:
        print(f"  FAIL {desc}")
        if detail:
            print(f"       {detail}")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    os.makedirs(CHAIN_DIR, exist_ok=True)
    with open(os.path.join(CHAIN_DIR, "_mixed.lc"), "w") as f:
        f.write(SRC)

    c = LspClient()
    c.open_doc()

    # ── Signature help on the mixed chains ──────────────────────────
    #   a.b.mid(1).d.run(2);        line 10
    #   items[0].mid(3).d.run(4);   line 12
    for line, col in [(10, 19), (12, 24)]:
        sigs = c.signature(line, col)
        check(f"signature run( mixed chain line {line+1}",
              any("(D) fn run" in s for s in sigs) and len(sigs) == 1,
              f"got {sigs}")

    # ── Completion after each '.' in the mixed chains ───────────────
    # a.b.mid(1).d.|  → members of D: n, run
    items = c.complete(10, 16)
    check("completion a.b.mid(1).d.| shows D members",
          "n" in items and "run" in items, f"got {items}")

    # items[0].c.|  → members of C: d
    items = c.complete(11, 13)
    check("completion items[0].c.| shows C member d",
          "d" in items, f"got {items}")

    # items[0].mid(3).|  → return of mid is C → member d
    items = c.complete(12, 18)
    check("completion items[0].mid(3).| shows C member d",
          "d" in items, f"got {items}")

    # ── Hover on deep members of the mixed chains ───────────────────
    # items[0].c.d.n  → hover on 'n' (line 11, col 13)
    raw = c.hover(11, 13)
    check("hover n of items[0].c.d.n",
          "int32" in raw or "field" in raw or "n" in raw, f"got '{raw[:120]}'")

    # a.b.mid(1).d.run  → hover on 'run' (line 10, col 16)
    raw = c.hover(10, 16)
    check("hover run of a.b.mid(1).d.run",
          "(D method)" in raw and "run" in raw, f"got '{raw[:120]}'")

    c.close()

    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed")
    print(f"{'='*50}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
