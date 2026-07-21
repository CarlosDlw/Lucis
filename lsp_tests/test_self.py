#!/usr/bin/env python3
"""Test the Self keyword in LSP features (hover, semantic tokens, signature help)."""

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

    def hover(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
            "params": {"textDocument": {"uri": uri}, "position": {"line": line, "character": col}}
        })
        return self._recv(timeout=timeout)

    def semanticTokens(self, source, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.1)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/semanticTokens/full",
            "params": {"textDocument": {"uri": uri}}
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


def test_semantic(description, source, expect_token_types):
    """Check that Self has the expected semantic token type."""
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
            if tokText == "Self":
                expected = expect_token_types.get("Self", -1)
                ok = (toktype == expected)
                if not ok:
                    print(f"       expected Self type={expected}, got type={toktype} at line {line}, col {col}")
                break

    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        if not resp or "result" not in resp:
            print(f"       no tokens returned")
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


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    # ── Semantic token tests ──
    print("── Semantic tokens ──")
    # Type = 13 per SemanticTokenType enum in SemanticTokensProvider.h
    # Keyword = 14
    semantic_tests = [
        ("Self in return type is highlighted as Keyword",
         'struct S {}\nextend S { fn new() Self { return S {}; } }',
         {"Self": 10}),  # Keyword = 10
    ]
    for desc, source, expect_types in semantic_tests:
        test_semantic(desc, source, expect_types)

    # ── Hover tests ──
    print("── Hover ──")
    hover_tests = [
        ("Hover on Self in return type shows S",
         'struct S {}\nextend S { fn new() Self { return S {}; } }',
         1, 20, "S"),
    ]
    for desc, source, line, col, expect in hover_tests:
        test_hover(desc, source, line, col, expect)

    total = len(semantic_tests) + len(hover_tests)
    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {total}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
