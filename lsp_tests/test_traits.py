#!/usr/bin/env python3
"""Test trait LSP features: completions and hover — local and cross-file."""

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
VIRTUAL_URI = "file:///tmp/_lsp_test.lc"


class LspClient:
    def __init__(self, root_uri=None):
        self.log = open(SERVER_LOG, "ab")
        self.proc = subprocess.Popen(
            [LUCIS_BIN, "lsp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, text=True, bufsize=0,
        )
        time.sleep(0.15)
        params = {"processId": None, "capabilities": {}}
        if root_uri:
            params["rootUri"] = root_uri
        self._send({
            "jsonrpc": "2.0", "id": 0, "method": "initialize",
            "params": params
        })
        self._recv(timeout=3)
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        if root_uri:
            time.sleep(0.5)
        else:
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

    def complete(self, source, line, col, timeout=10):
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": VIRTUAL_URI, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.3)
        # Drain any diagnostic notifications
        self._recv(timeout=2)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
            "params": {"textDocument": {"uri": VIRTUAL_URI}, "position": {"line": line, "character": col}}
        })
        return self._recv(timeout=timeout)

    def hover(self, source, line, col, timeout=10):
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": VIRTUAL_URI, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.3)
        # Drain any diagnostic notifications
        self._recv(timeout=2)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
            "params": {"textDocument": {"uri": VIRTUAL_URI}, "position": {"line": line, "character": col}}
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

    def definition(self, source, line, col, timeout=10):
        self._send({"jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": VIRTUAL_URI, "languageId": "lucis", "version": 1, "text": source}}})
        time.sleep(0.3)
        self._recv(timeout=2)
        self._send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/definition",
            "params": {"textDocument": {"uri": VIRTUAL_URI}, "position": {"line": line, "character": col}}})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            r = self._recv(timeout=2)
            if r and "id" in r:
                return r
        return None

    def signatureHelp(self, source, line, col, timeout=10):
        self._send({"jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": VIRTUAL_URI, "languageId": "lucis", "version": 1, "text": source}}})
        time.sleep(0.3)
        self._recv(timeout=2)
        self._send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/signatureHelp",
            "params": {"textDocument": {"uri": VIRTUAL_URI}, "position": {"line": line, "character": col}}})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            r = self._recv(timeout=2)
            if r and "id" in r:
                return r
        return None

    def semanticTokens(self, source, timeout=10):
        self._send({"jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": VIRTUAL_URI, "languageId": "lucis", "version": 1, "text": source}}})
        time.sleep(0.3)
        self._recv(timeout=2)
        self._send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/semanticTokens/full",
            "params": {"textDocument": {"uri": VIRTUAL_URI}}})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            r = self._recv(timeout=2)
            if r and "id" in r:
                return r
        return None


def test_completion(description, source, line, col, expect_any=None, root_uri=None):
    global passed, failed
    expect_any = expect_any or []
    client = LspClient(root_uri=root_uri)
    try:
        resp = client.complete(source, line, col, timeout=8)
    finally:
        client.close()
    labels = [i["label"] for i in resp["result"]] if resp and "result" in resp else []
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


def test_hover(description, source, line, col, expect_contains, root_uri=None):
    global passed, failed
    client = LspClient(root_uri=root_uri)
    try:
        resp = client.hover(source, line, col, timeout=8)
    finally:
        client.close()
    raw = ""
    if resp and "result" in resp and resp["result"]:
        c = resp["result"]["contents"]
        if isinstance(c, dict):
            raw = c.get("value", "")
        elif isinstance(c, list) and len(c) > 0:
            raw = c[0].get("value", "") if isinstance(c[0], dict) else ""
        else:
            raw = str(c) if c else ""
    ok = expect_contains in raw
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        print(f"       expected '{expect_contains}' in '{raw[:200]}'")
        failed += 1


def test_definition(description, source, line, col, expect_line, root_uri=None):
    global passed, failed
    client = LspClient(root_uri=root_uri)
    try:
        resp = client.definition(source, line, col, timeout=8)
    finally:
        client.close()
    got_line = -1
    if resp and "result" in resp and resp["result"]:
        r = resp["result"]
        if isinstance(r, list) and len(r) > 0:
            r = r[0]
        if isinstance(r, dict):
            got_line = r.get("range", {}).get("start", {}).get("line", -1)
    ok = got_line == expect_line
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        print(f"       expected line {expect_line}, got {got_line}")
        failed += 1


def test_signature(description, source, line, col, expect_contains, root_uri=None):
    global passed, failed
    client = LspClient(root_uri=root_uri)
    try:
        resp = client.signatureHelp(source, line, col, timeout=8)
    finally:
        client.close()
    labels = []
    if resp and "result" in resp and resp["result"]:
        labels = [s.get("label", "") for s in resp["result"].get("signatures", [])]
    ok = any(expect_contains in s for s in labels)
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        print(f"       expected '{expect_contains}' in {labels}")
        failed += 1


def test_semantic(description, source, keyword):
    global passed, failed
    client = LspClient()
    try:
        resp = client.semanticTokens(source, timeout=8)
    finally:
        client.close()
    ok = False
    KW = 10
    if resp and "result" in resp and resp["result"]:
        data = resp["result"]["data"]
        line = 0; col = 0
        for i in range(0, len(data), 5):
            dline = data[i]; dcol = data[i+1]; length = data[i+2]; toktype = data[i+3]
            if dline > 0: line += dline; col = dcol
            else: col += dcol
            lines = source.split('\n')
            if line < len(lines) and col + length <= len(lines[line]):
                tokText = lines[line][col:col+length]
            else:
                tokText = ""
            if tokText == keyword:
                ok = (toktype == KW)
                break
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try: os.remove(SERVER_LOG)
    except OSError: pass

    # ══════════════════════════════════════════════════════════════
    #  LOCAL TESTS — fresh client per test, no project
    # ══════════════════════════════════════════════════════════════

    print("═══ Local completions ═══")

    test_completion(
        "local: impl| completes trait names",
        "trait Hashable { fn hash(&self) int32; }\n"
        "trait Default { fn default() int32; }\n"
        "struct S {}\n"
        "impl H\n",
        3, 5, ["Hashable", "Default"])

    test_completion(
        "local: type position completes trait name",
        "trait Greeter { fn greet(&self) int32; }\n"
        "Gre\n",
        1, 3, ["Greeter"])

    test_completion(
        "local: p.gr| completes trait method 'greet'",
        "trait Greeter { fn greet(&self) int32; }\n"
        "struct Person {}\n"
        "impl Greeter for Person {\n"
        "    fn greet(&self) int32 { ret 42; }\n"
        "}\n"
        "fn main() int32 {\n"
        "    Person p;\n"
        "    p.gr\n"
        "    ret 0;\n"
        "}\n",
        7, 6, ["greet"])

    test_completion(
        "local: s.| completes 'foo' and 'bar'",
        "trait A { fn foo(&self) int32; }\n"
        "trait B { fn bar(&self) int32; }\n"
        "struct S {}\n"
        "impl A for S { fn foo(&self) int32 { ret 1; } }\n"
        "impl B for S { fn bar(&self) int32 { ret 2; } }\n"
        "fn main() int32 {\n"
        "    S s;\n"
        "    s.\n"
        "    ret 0;\n"
        "}\n",
        7, 6, ["foo", "bar"])

    print("═══ Local hover ═══")

    test_hover(
        "local: hover on trait name 'Hashable'",
        "trait Hashable {\n"
        "    fn hash(&self) int32;\n"
        "}\n",
        0, 8, "trait Hashable")

    test_hover(
        "local: hover shows method 'hash'",
        "trait Hashable {\n"
        "    fn hash(&self) int32;\n"
        "}\n",
        0, 8, "fn hash")

    test_hover(
        "local: hover on generic trait 'From'",
        "trait From<T> {\n"
        "    fn from(T value) int32;\n"
        "}\n",
        0, 8, "trait From<T>")

    test_hover(
        "local: hover shows 'fn from'",
        "trait From<T> {\n"
        "    fn from(T value) int32;\n"
        "}\n",
        0, 8, "fn from")

    # ══════════════════════════════════════════════════════════════
    #  CROSS-FILE TESTS — fresh client with project rootUri
    # ══════════════════════════════════════════════════════════════

    ROOT_URI = "file://" + PROJECT_DIR

    print("═══ Cross-file completions ═══")

    test_completion(
        "cross: impl| completes cross-file 'Hashable'",
        "use traits::Hashable;\n"
        "struct Val {}\n"
        "impl Ha\n",
        2, 5, ["Hashable"], root_uri=ROOT_URI)

    test_completion(
        "cross: impl| completes cross-file 'Counter'",
        "use traits::Counter;\n"
        "struct Val {}\n"
        "impl Co\n",
        2, 5, ["Counter"], root_uri=ROOT_URI)

    test_completion(
        "cross: impl| completes cross-file 'From' (generic)",
        "use traits::From;\n"
        "struct Val {}\n"
        "impl Fr\n",
        2, 5, ["From"], root_uri=ROOT_URI)

    print("═══ Cross-file hover ═══")

    test_hover(
        "cross: hover on 'Hashable' in impl",
        "use traits::Hashable;\n"
        "struct Val {}\n"
        "impl Hashable for Val {\n"
        "    fn hash(&self) int32 { ret 42; }\n"
        "}\n",
        2, 5, "trait Hashable", root_uri=ROOT_URI)

    test_hover(
        "cross: hover on 'Counter' in impl",
        "use traits::Counter;\n"
        "struct Val {}\n"
        "impl Counter for Val {\n"
        "    fn count(&self) int32 { ret 0; }\n"
        "    fn incr(&self) void {}\n"
        "}\n",
        2, 5, "trait Counter", root_uri=ROOT_URI)

    # ── Trait method hover ──
    print("═══ Trait method hover ═══")

    test_hover(
        "local: hover on trait method call 'hash'",
        "trait Hashable { fn hash(&self) int32; }\n"
        "struct S {}\n"
        "impl Hashable for S {\n"
        "    fn hash(&self) int32 { ret 42; }\n"
        "}\n"
        "fn main() int32 {\n"
        "    S s;\n"
        "    ret s.hash();\n"
        "}\n",
        7, 13, "trait method")

    test_hover(
        "cross: hover on trait method call 'hash'",
        "use traits::Hashable;\n"
        "struct Val {}\n"
        "impl Hashable for Val {\n"
        "    fn hash(&self) int32 { ret 42; }\n"
        "}\n"
        "fn main() int32 {\n"
        "    Val v;\n"
        "    ret v.hash();\n"
        "}\n",
        7, 13, "trait method", root_uri=ROOT_URI)

    # ── Cross-file trait method completions ──
    print("═══ Cross-file method completions ═══")

    test_completion(
        "cross: v.| completes trait method 'hash'",
        "use traits::Hashable;\n"
        "struct Val {}\n"
        "impl Hashable for Val { fn hash(&self) int32 { ret 42; } }\n"
        "fn main() int32 {\n"
        "    Val v;\n"
        "    v.\n"
        "    ret 0;\n"
        "}\n",
        5, 6, ["hash"], root_uri=ROOT_URI)

    # ── Go-to-definition ──
    print("═══ Go-to-definition ═══")

    test_definition(
        "local: go-to-def on trait name",
        "trait Greeter { fn greet(&self) int32; }\n"
        "struct S {}\n"
        "impl Greeter for S {\n"
        "    fn greet(&self) int32 { ret 0; }\n"
        "}\n",
        2, 6, 0)  # 'Greeter' on impl line col 6 → trait at line 0

    test_definition(
        "cross: go-to-def on cross-file trait name",
        "use traits::Hashable;\n"
        "struct Val {}\n"
        "impl Hashable for Val { fn hash(&self) int32 { ret 42; } }\n",
        2, 5, 1, root_uri=ROOT_URI)  # traits.lc line 1 (0-based)

    # ── Signature help ──
    print("═══ Signature help ═══")

    test_signature(
        "local: signature help for trait method call",
        "trait Greeter { fn greet(&self, int32 n) int32; }\n"
        "struct S {}\n"
        "impl Greeter for S { fn greet(&self, int32 n) int32 { ret n; } }\n"
        "fn main() int32 { S s; s.greet(42); }\n",
        3, 31, "greet")

    # ── Semantic tokens ──
    print("═══ Semantic tokens ═══")

    test_semantic(
        "'trait' keyword is classified as Keyword",
        "trait Hashable { fn hash(&self) int32; }\n",
        "trait")

    test_semantic(
        "'impl' keyword is classified as Keyword",
        "struct S {}\nimpl Hashable for S { fn hash(&self) int32 { ret 0; } }\n",
        "impl")

    # ── Remaining Phase 7.1 completions ──
    print("═══ Extra completions ═══")

    test_completion(
        "cross: T: | constraint completes trait names",
        "use traits::Hashable;\n"
        "fn foo<T: Ha\n",
        1, 11, ["Hashable"], root_uri=ROOT_URI)

    # ── Hover on imported traits in use declarations ──
    print("═══ Hover on use imports ═══")

    test_hover(
        "cross: hover on 'Hashable' in use import",
        "use traits::Hashable;\n"
        "struct Val {}\n",
        0, 13, "trait Hashable", root_uri=ROOT_URI)

    test_hover(
        "cross: hover on 'Counter' in use import",
        "use traits::Counter;\n"
        "struct Val {}\n",
        0, 13, "trait Counter", root_uri=ROOT_URI)

    # ── Static trait method completions ──
    print("═══ Static method completions ═══")

    test_completion(
        "local: Type::| completes static trait method",
        "trait From<T> { fn from(T value) int32; }\n"
        "struct Val {}\n"
        "impl From<int32> for Val { fn from(int32 value) int32 { ret value; } }\n"
        "fn main() int32 { Val::fr\n}\n",
        3, 23, ["from"])

    test_completion(
        "cross: Type::| completes static trait method",
        "use traits::From;\n"
        "struct Val {}\n"
        "impl From<int32> for Val { fn from(int32 value) int32 { ret value; } }\n"
        "fn main() int32 { Val::fr\n}\n",
        3, 23, ["from"], root_uri=ROOT_URI)

    # ── Constrained generic completions + hover ──
    print("═══ Constrained generic ═══")

    test_completion(
        "local: val.| on constrained generic completes trait methods",
        "trait Hashable { fn hash(&self) int32; }\n"
        "fn printHash<T: Hashable>(T val) void {\n"
        "    val.h\n"
        "}\n",
        2, 8, ["hash"])

    test_hover(
        "local: hover on constrained param shows constraint",
        "trait Hashable { fn hash(&self) int32; }\n"
        "fn printHash<T: Hashable>(T val) void {\n"
        "}\n",
        1, 30,         "(parameter Hashable) T")

    total = passed + failed
    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {total}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
