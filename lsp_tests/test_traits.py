#!/usr/bin/env python3
"""Test trait LSP features: completions and hover — both local and cross-file."""

import subprocess
import json
import sys
import os
import time
import select

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_stderr.log")
PROJECT_DIR = os.path.join(os.path.dirname(__file__), "traits", "project")

passed = 0
failed = 0


class LspClient:
    def __init__(self, root_uri=None):
        self.log = open(SERVER_LOG, "ab")
        self.proc = subprocess.Popen(
            [LUCIS_BIN, "lsp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, text=True, bufsize=0,
        )
        time.sleep(0.15)
        self.msgId = 0
        self._init(root_uri)

    def _init(self, root_uri=None):
        params = {"processId": None, "capabilities": {}}
        if root_uri:
            params["rootUri"] = root_uri
        self._send({
            "jsonrpc": "2.0", "id": 0, "method": "initialize",
            "params": params
        })
        self._recv(timeout=3)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        time.sleep(0.5)

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

    def _nextId(self):
        self.msgId += 1
        return self.msgId

    def openFile(self, uri, source):
        self._send({
            "jsonrpc": "2.0", "id": self._nextId(), "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.15)

    def completions(self, uri, line, col, timeout=10):
        self._send({
            "jsonrpc": "2.0", "id": self._nextId(), "method": "textDocument/completion",
            "params": {"textDocument": {"uri": uri}, "position": {"line": line, "character": col}}
        })
        resp = self._recv(timeout=timeout)
        labels = []
        if resp and "result" in resp and resp["result"]:
            items = resp["result"]
            if isinstance(items, list):
                labels = [i.get("label", "") for i in items]
            elif isinstance(items, dict) and "items" in items:
                labels = [i.get("label", "") for i in items["items"]]
        return labels

    def hover(self, uri, line, col, timeout=10):
        self._send({
            "jsonrpc": "2.0", "id": self._nextId(), "method": "textDocument/hover",
            "params": {"textDocument": {"uri": uri}, "position": {"line": line, "character": col}}
        })
        resp = self._recv(timeout=timeout)
        if resp and "result" in resp and resp["result"]:
            c = resp["result"]["contents"]
            if isinstance(c, dict):
                return c.get("value", "")
            elif isinstance(c, list) and len(c) > 0:
                if isinstance(c[0], dict):
                    return c[0].get("value", "")
            return c if isinstance(c, str) else ""
        return ""

    def close(self):
        try:
            self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
            self._recv(timeout=2)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()
        self.log.close()


def check_completion(desc, labels, expect_any):
    global passed, failed
    ok = all(e in labels for e in expect_any) if expect_any else True
    if ok:
        print(f"  OK   {desc}")
        passed += 1
    else:
        print(f"  FAIL {desc}")
        missing = [e for e in expect_any if e not in labels]
        print(f"       missing: {missing}")
        print(f"       got: {labels[:30]}")
        failed += 1


def check_hover(desc, text, expect_contains):
    global passed, failed
    ok = expect_contains in text
    if ok:
        print(f"  OK   {desc}")
        passed += 1
    else:
        print(f"  FAIL {desc}")
        print(f"       expected '{expect_contains}' in '{text[:200]}'")
        failed += 1


# ── inline source helper ──────────────────────────────────────────────

VIRTUAL_URI = "file:///tmp/_lsp_test.lc"

def inline_completions(client, source, line, col):
    client.openFile(VIRTUAL_URI, source)
    return client.completions(VIRTUAL_URI, line, col)

def inline_hover(client, source, line, col):
    client.openFile(VIRTUAL_URI, source)
    return client.hover(VIRTUAL_URI, line, col)


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    # ══════════════════════════════════════════════════════════════
    #  LOCAL TESTS (inline source, no project)
    # ══════════════════════════════════════════════════════════════

    client = LspClient()
    try:
        print("═══ Local completions ═══")

        src = ("trait Hashable { fn hash(&self) int32; }\n"
               "trait Default { fn default() int32; }\n"
               "struct S {}\n"
               "impl H\n")
        labels = inline_completions(client, src, 3, 5)
        check_completion("local: impl| completes trait names",
                         labels, ["Hashable", "Default"])

        src = ("trait Greeter { fn greet(&self) int32; }\n"
               "Gre\n")
        labels = inline_completions(client, src, 1, 3)
        check_completion("local: type position completes trait name",
                         labels, ["Greeter"])

        src = ("trait Greeter { fn greet(&self) int32; }\n"
               "struct Person {}\n"
               "impl Greeter for Person {\n"
               "    fn greet(&self) int32 { ret 42; }\n"
               "}\n"
               "fn main() int32 {\n"
               "    Person p;\n"
               "    p.gr\n"
               "    ret 0;\n"
               "}\n")
        labels = inline_completions(client, src, 7, 6)
        check_completion("local: p.gr| completes trait method 'greet'",
                         labels, ["greet"])

        src = ("trait A { fn foo(&self) int32; }\n"
               "trait B { fn bar(&self) int32; }\n"
               "struct S {}\n"
               "impl A for S { fn foo(&self) int32 { ret 1; } }\n"
               "impl B for S { fn bar(&self) int32 { ret 2; } }\n"
               "fn main() int32 {\n"
               "    S s;\n"
               "    s.\n"
               "    ret 0;\n"
               "}\n")
        labels = inline_completions(client, src, 7, 6)
        check_completion("local: s.| completes 'foo' and 'bar'",
                         labels, ["foo", "bar"])

        print("═══ Local hover ═══")

        src = ("trait Hashable {\n"
               "    fn hash(&self) int32;\n"
               "}\n")
        h = inline_hover(client, src, 0, 8)
        check_hover("local: hover on trait name 'Hashable'", h, "trait Hashable")
        check_hover("local: hover shows method 'hash'", h, "fn hash")

        src = ("trait From<T> {\n"
               "    fn from(T value) int32;\n"
               "}\n")
        h = inline_hover(client, src, 0, 8)
        check_hover("local: hover on generic trait 'From'", h, "trait From<T>")
        check_hover("local: hover shows 'fn from'", h, "fn from")
    finally:
        client.close()

    # ══════════════════════════════════════════════════════════════
    #  CROSS-FILE TESTS (project with traits.lc + main.lc)
    # ══════════════════════════════════════════════════════════════

    root_uri = "file://" + PROJECT_DIR
    main_uri = "file://" + os.path.join(PROJECT_DIR, "src", "main.lc")
    traits_uri = "file://" + os.path.join(PROJECT_DIR, "src", "traits.lc")

    client = LspClient(root_uri=root_uri)
    for path, uri in [
        (os.path.join(PROJECT_DIR, "src", "main.lc"), main_uri),
        (os.path.join(PROJECT_DIR, "src", "traits.lc"), traits_uri),
    ]:
        with open(path) as f:
            client.openFile(uri, f.read())
    time.sleep(0.5)
    for _ in range(20):
        client._recv(timeout=0.3)

    try:
        print("═══ Cross-file completions ═══")

        # Trait name in type position — cross-file trait
        src = ("use traits::{ Hashable, Counter };\n"
               "struct Val {}\n"
               "impl Co\n")
        labels = inline_completions(client, src, 2, 5)
        check_completion("cross: impl| completes cross-file 'Counter'",
                         labels, ["Counter"])

        # Trait name in type position — cross-file generic trait
        src = ("use traits::{ From };\n"
               "struct Val {}\n"
               "impl Fr\n")
        labels = inline_completions(client, src, 2, 5)
        check_completion("cross: impl| completes cross-file 'From'",
                         labels, ["From"])

        print("═══ Cross-file hover ═══")

        # Hover on cross-file trait name in impl declaration
        src = ("use traits::{ Hashable, Counter };\n"
               "struct Val {}\n"
               "impl Hashable for Val {\n"
               "    fn hash(&self) int32 { ret 42; }\n"
               "}\n")
        h = inline_hover(client, src, 2, 5)
        check_hover("cross: hover on 'Hashable' in impl",
                    h, "trait Hashable")

    finally:
        client.close()
        os.system("pkill -f 'lucis lsp' 2>/dev/null")

    total = passed + failed
    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {total}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
