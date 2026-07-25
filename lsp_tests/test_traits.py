#!/usr/bin/env python3
"""Test trait completions via lucis LSP."""

import subprocess
import json
import sys
import os
import time
import select

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_stderr.log")

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
        self._send({
            "jsonrpc": "2.0", "id": 0, "method": "initialize",
            "params": {"processId": None, "rootUri": "file:///tmp", "capabilities": {}}
        })
        self._recv(timeout=3)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        time.sleep(0.1)

    def _send(self, obj):
        body = json.dumps(obj, ensure_ascii=False)
        self.proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
        self.proc.stdin.flush()

    def _recv(self, timeout=5):
        raw = self.proc.stdout.buffer
        deadline = time.monotonic() + timeout
        line = b""
        while time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.1, deadline - time.monotonic()))
            if r:
                ch = raw.read(1)
                if not ch: return None
                line += ch
                if line.endswith(b"\r\n"): break
            else: return None
        hdr = line.decode().strip()
        if not hdr.startswith("Content-Length:"): return None
        n = int(hdr.split(":")[1].strip())
        raw.read(2)
        body = b""
        while len(body) < n:
            chunk = raw.read(n - len(body))
            if not chunk: break
            body += chunk
        return json.loads(body)

    def completions(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
            "params": {"textDocument": {"uri": uri}, "position": {"line": line, "character": col}}
        })
        return self._recv(timeout=timeout)

    def close(self):
        try:
            self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
            self._recv(timeout=2)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()
        self.log.close()


def test_completion(description, source, line, col, expect_any=None):
    global passed, failed
    expect_any = expect_any or []

    client = LspClient()
    try:
        resp = client.completions(source, line, col, timeout=8)
    finally:
        client.close()

    labels = []
    if resp and "result" in resp and resp["result"]:
        items = resp["result"]
        if isinstance(items, list):
            labels = [i.get("label", "") for i in items]
        elif isinstance(items, dict) and "items" in items:
            labels = [i.get("label", "") for i in items["items"]]

    ok = all(e in labels for e in expect_any) if expect_any else True
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        missing = [e for e in expect_any if e not in labels]
        print(f"       missing: {missing}")
        print(f"       got: {labels[:30]}")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    # ── Trait name completions ────────────────────────────────────
    print("── Trait name completions ──")

    # In type position (general context also shows type names)
    src1 = ("trait Hashable { fn hash(&self) int32; }\n"
            "trait Default { fn default() int32; }\n"
            "struct S {}\n"
            "impl H\n")
    test_completion("impl| completes trait names",
                    src1, 3, 5, ["Hashable", "Default"])

    # Trait names should appear in general context too
    src2 = ("trait Greeter { fn greet(&self) int32; }\n"
            "Gre\n")
    test_completion("type position completes trait name",
                    src2, 1, 3, ["Greeter"])

    # ── Trait method completions on dot access ────────────────────
    print("── Trait method completions ──")

    src3 = ("trait Greeter { fn greet(&self) int32; }\n"
            "struct Person {}\n"
            "impl Greeter for Person {\n"
            "    fn greet(&self) int32 { ret 42; }\n"
            "}\n"
            "fn main() int32 {\n"
            "    Person p;\n"
            "    p.gr\n"
            "    ret 0;\n"
            "}\n")
    test_completion("p.gr| completes trait method 'greet'",
                    src3, 7, 6, ["greet"])

    # Trait method with prefix filter
    src4 = ("trait Math { fn add(&self, int32 a, int32 b) int32; }\n"
            "struct Val { int32 x; }\n"
            "impl Math for Val {\n"
            "    fn add(&self, int32 a, int32 b) int32 { ret a + b; }\n"
            "}\n"
            "fn main() int32 {\n"
            "    Val v;\n"
            "    v.a\n"
            "    ret 0;\n"
            "}\n")
    test_completion("v.a| completes trait method 'add'",
                    src4, 7, 6, ["add"])

    # Multiple traits on same type
    src5 = ("trait A { fn foo(&self) int32; }\n"
            "trait B { fn bar(&self) int32; }\n"
            "struct S {}\n"
            "impl A for S { fn foo(&self) int32 { ret 1; } }\n"
            "impl B for S { fn bar(&self) int32 { ret 2; } }\n"
            "fn main() int32 {\n"
            "    S s;\n"
            "    s.\n"
            "    ret 0;\n"
            "}\n")
    test_completion("s.| completes 'foo' and 'bar' from multiple traits",
                    src5, 7, 6, ["foo", "bar"])

    total = passed + failed
    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {total}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
