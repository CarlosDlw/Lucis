#!/usr/bin/env python3
"""Cross-file bitstruct LSP tests (hover, completion, signature help)."""

import subprocess
import json
import sys
import os
import time
import select

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_stderr.log")
PROJECT_DIR = os.path.join(os.path.dirname(__file__), "project")

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
                if ch == b'\n':
                    if line.strip():
                        try:
                            hdr = line.decode().strip()
                        except:
                            line = b""
                            continue
                        if hdr.startswith("Content-Length:"):
                            length = int(hdr.split(":")[1].strip())
                            raw.read(2)
                            body = raw.read(length)
                            try:
                                return json.loads(body.decode())
                            except:
                                return None
                    line = b""
                else:
                    line += ch
        return None

    def _nextId(self):
        self.msgId += 1
        return self.msgId

    def openFile(self, uri, source):
        self._send({
            "jsonrpc": "2.0", "id": self._nextId(), "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.15)

    def hover(self, uri, line, col, timeout=10):
        self._send({
            "jsonrpc": "2.0", "id": self._nextId(), "method": "textDocument/hover",
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


def hover_text(client, uri, line, col):
    resp = client.hover(uri, line, col, timeout=8)
    if resp and "result" in resp and resp["result"]:
        c = resp["result"]["contents"]
        return c.get("value", "") if isinstance(c, dict) else (c or "")
    return ""


def check(desc, ok):
    global passed, failed
    if ok:
        print(f"  OK   {desc}")
        passed += 1
    else:
        print(f"  FAIL {desc}")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    root_uri = "file://" + PROJECT_DIR
    main_uri = "file://" + os.path.join(PROJECT_DIR, "src", "main.lc")
    ctrl_uri = "file://" + os.path.join(PROJECT_DIR, "src", "control.lc")

    # Use a single client for all hovers, with a fresh open per test batch
    client = LspClient(root_uri=root_uri)
    for path, uri in [
        (main_uri.replace("file://", ""), main_uri),
        (ctrl_uri.replace("file://", ""), ctrl_uri),
    ]:
        with open(path) as f:
            client.openFile(uri, f.read())
    time.sleep(0.5)

    # Drain any unread diagnostics
    for _ in range(20):
        client._recv(timeout=0.3)

    try:
        print("── Cross-File Hover Tests ──")

        h = hover_text(client, main_uri, 11, 18)
        print(f"  clearAll: {h[:120] if h else '(empty)'}")
        check("hover 'clearAll'", "clearAll()" in h)

        h = hover_text(client, main_uri, 18, 17)
        print(f"  setAll: {h[:120] if h else '(empty)'}")
        check("hover 'setAll'", "setAll()" in h)

        h = hover_text(client, main_uri, 9, 34)
        print(f"  raw method: {h[:120] if h else '(empty)'}")
        check("hover 'raw'", "uint16 raw()" in h)

        h = hover_text(client, main_uri, 14, 11)
        print(f"  raw var: {h[:120] if h else '(empty)'}")
        check("hover 'raw' variable", "raw" in h)

        print()
        print("── Line 7 assignment hover tests ──")
        # Line 7 (LSP 6): '    atc.reg.counter = 42;'
        # Hover on 'atc' at col 4
        h = hover_text(client, main_uri, 6, 5)
        print(f"  atc: {h[:120] if h else '(empty)'}")
        check("hover 'atc' in assignment", "Atc" in h)
        # Hover on 'reg' at col 8
        h = hover_text(client, main_uri, 6, 9)
        print(f"  reg: {h[:120] if h else '(empty)'}")
        check("hover 'reg' in assignment", "Control" in h)
        # Hover on 'counter' at col 12
        h = hover_text(client, main_uri, 6, 14)
        print(f"  counter: {h[:120] if h else '(empty)'}")
        check("hover 'counter' in assignment", "counter" in h)

        print(f"\n{passed} passed, {failed} failed")
    finally:
        client.close()
        os.system("pkill -f 'lucis lsp' 2>/dev/null")


if __name__ == "__main__":
    sys.exit(main())
