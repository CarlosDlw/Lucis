#!/usr/bin/env python3
"""Test @cfg(...) LSP features (completions, semantic tokens, hover) via lucis LSP."""

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
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self.log,
            text=True,
            bufsize=0,
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

    def complete(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri, "languageId": "lucis", "version": 1, "text": source
                }
            }
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
            "params": {
                "textDocument": {"uri": uri},
                "position": {"line": line, "character": col}
            }
        })
        return self._recv(timeout=timeout)

    def hover(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri, "languageId": "lucis", "version": 1, "text": source
                }
            }
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
            "params": {
                "textDocument": {"uri": uri},
                "position": {"line": line, "character": col}
            }
        })
        return self._recv(timeout=timeout)

    def semanticTokens(self, source, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri, "languageId": "lucis", "version": 1, "text": source
                }
            }
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/semanticTokens/full",
            "params": {
                "textDocument": {"uri": uri}
            }
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


def test_completion(description, source, line, col, expect_any=None, expect_none=None):
    global passed, failed
    expect_any = expect_any or []
    expect_none = expect_none or []

    client = LspClient()
    try:
        resp = client.complete(source, line, col, timeout=8)
    finally:
        client.close()

    labels = [i["label"] for i in resp["result"]] if resp and "result" in resp else []
    any_ok = any(e in labels for e in expect_any) or not expect_any
    none_ok = all(e not in labels for e in expect_none) or not expect_none

    if any_ok and none_ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        if not any_ok:
            print(f"       expected any of {expect_any}, got {[l for l in labels if l in expect_any + expect_none]}")
        if not none_ok:
            print(f"       unexpected {[e for e in expect_none if e in labels]}")
        failed += 1


def test_hover(description, source, line, col, expect_contains):
    global passed, failed
    client = LspClient()
    try:
        resp = client.hover(source, line, col, timeout=8)
    finally:
        client.close()

    raw = ""
    if resp and "result" in resp and resp["result"]:
        c = resp["result"]["contents"]
        if isinstance(c, dict):
            raw = c.get("value", "")
        else:
            raw = c or ""
    ok = expect_contains in raw
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        print(f"       expected '{expect_contains}' in '{raw[:200]}'")
        failed += 1


def test_semantic(description, source, expect_token_types):
    """Check that the @cfg identifier has the expected semantic token type."""
    global passed, failed
    client = LspClient()
    try:
        resp = client.semanticTokens(source, timeout=8)
    finally:
        client.close()

    ok = False
    if resp and "result" in resp and resp["result"]:
        data = resp["result"]["data"]
        line = 0
        col = 0
        for i in range(0, len(data), 5):
            dline = data[i]
            dcol = data[i+1]
            length = data[i+2]
            toktype = data[i+3]
            modifiers = data[i+4]
            if dline > 0:
                line += dline
                col = dcol
            else:
                col += dcol
            lines = source.split('\n')
            if line < len(lines) and col + length <= len(lines[line]):
                tokText = lines[line][col:col+length]
            else:
                tokText = ""
            if tokText == "cfg":
                expected = expect_token_types.get("cfg", -1)
                ok = (toktype == expected)
                if not ok:
                    print(f"       expected cfg type={expected}, got type={toktype} (modifiers={modifiers}) at line {line}, col {col}")
                break

    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        if not resp or "result" not in resp:
            print(f"       no tokens returned")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    # ── Completion tests ───────────────────────────────────────
    print("── Completion ──")
    completion_tests = [
        ("@ → cfg completion", "@", 0, 1, ["cfg"], []),
        ("@c → cfg completion", "@c", 0, 2, ["cfg"], []),
        ("@cfg( → shows keys", "@cfg(", 0, 5, ["target_os", "target_arch"], []),
        ("@cfg( → shows shorthands", "@cfg(", 0, 5, ["unix", "windows"], []),
        ("@cfg( → shows all/any/not", "@cfg(", 0, 5, ["all", "any", "not"], []),
        ("@cfg(target_ → filters keys", "@cfg(target_", 0, 12,
         ["target_os", "target_arch"], ["unix", "windows"]),
        ("@cfg(debug_ → filters debug", "@cfg(debug_", 0, 12,
         ["debug_assertions"], ["target_os"]),
        ("@cfg(target_os = \" → values", '@cfg(target_os = "', 0, 19,
         ['"linux"', '"windows"'], ['"x86_64"']),
        ("@cfg(target_endian = \" → endian values", '@cfg(target_endian = "', 0, 23,
         ['"little"', '"big"'], ['"linux"']),
        ("@cfg(unix → shows unix shorthand", "@cfg(unix", 0, 9,
         ["unix"], ["target_os"]),
        ("@cfg(all( → shows keys nested", "@cfg(all(", 0, 9,
         ["target_os", "unix", "all"], []),
        ("@cfg(target_os = \"li → filters value prefix", '@cfg(target_os = "li', 0, 20,
         ['"linux"'], ['"windows"']),
    ]
    for args in completion_tests:
        test_completion(*args)

    # ── Hover tests ────────────────────────────────────────────
    print("── Hover ──")
    hover_tests = [
        ("Hover on cfg in if", 'fn main() int32 { if @cfg(true) { ret 0; } }', 0, 22, "@cfg(…)"),
        ("Hover on cfg in expr", 'fn main() int32 { bool b = @cfg(target_os == "linux"); }', 0, 30, "@cfg(…)"),
    ]
    for desc, source, line, col, expect in hover_tests:
        test_hover(
            f"Hover: '{source[:20]}' @ {line}:{col}",
            source, line, col, expect
        )

    # ── Semantic token tests ───────────────────────────────────
    print("── Semantic tokens ──")
    # Decorator = 17 per SemanticTokenType enum in SemanticTokensProvider.h
    semantic_tests = [
        ("@cfg identifier is Decorator", 'fn main() int32 { if @cfg(true) { ret 0; } }', {"cfg": 17}),
        ("@cfg in let expr is Decorator", 'fn main() int32 { bool b = @cfg(target_os == "linux"); }', {"cfg": 17}),
    ]
    for desc, source, expect_types in semantic_tests:
        test_semantic(desc, source, expect_types)

    total = len(completion_tests) + len(hover_tests) + len(semantic_tests)
    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {total}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
