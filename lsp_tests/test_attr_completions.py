#!/usr/bin/env python3
"""Test #[cfg(...)] attribute completions via lucis LSP.

Runs each test as an independent LSP session to avoid state leakage.
"""

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
    """Manages one LSP session (start → exchange → shutdown → wait)."""

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
        """Read one JSON-RPC message."""
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

    def drain_notifications(self, timeout=2):
        """Read and discard server notifications until a request arrives or timeout."""
        while True:
            m = self._recv(timeout=timeout)
            if m is None:
                return
            if "id" in m:
                # It's a response, return it
                return m

    def complete(self, source, line, col, timeout=10):
        """Open a document and request completions."""
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

    def close(self):
        try:
            self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
            self._recv(timeout=2)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()
        self.log.close()


def test(description, source, line, col, expect_any=None, expect_none=None):
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
            print(f"       expected any of {expect_any} in {[l for l in labels if 'cfg' in l or any(x in l for x in expect_any)]}")
            if not any(c in labels for c in expect_any):
                print(f"       (full list: {[l for l in labels if l in ['cfg'] + expect_any + expect_none]})")
        if not none_ok:
            print(f"       unexpected {[e for e in expect_none if e in labels]}")
        failed += 1


def main():
    global passed, failed
    # Clean up any strays
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    tests = [
        ("Type: '#[' → shows 'cfg'", "#[", 0, 2, ["cfg"]),
        ("Type: '#[' → shows 'deprecated'", "#[", 0, 2, ["deprecated"]),
        ("Type: '#[' → shows 'repr(C)'", "#[", 0, 2, ["repr(C)"]),
        ("Type: '#[cfg' → offers 'cfg'", "#[cfg", 0, 5, ["cfg"]),
        ("Type: '#[cfg(' → shows keys", "#[cfg(", 0, 6, ["target_os", "target_arch"]),
        ("Type: '#[cfg(' → shows shorthands", "#[cfg(", 0, 6, ["unix", "windows"]),
        ("Type: '#[cfg(' → shows all/any/not", "#[cfg(", 0, 6, ["all", "any", "not"]),
        ("Type: '#[cfg(target_' → filters keys", "#[cfg(target_", 0, 13,
         ["target_os", "target_arch", "target_endian"], ["unix", "windows"]),
        ("Type: '#[cfg(debug_' → filters keys", "#[cfg(debug_", 0, 13,
         ["debug_assertions"], ["target_os", "unix"]),
        ("Type: '#[cfg(target_os = \"' → values", '#[cfg(target_os = "', 0, 20,
         ['"linux"', '"windows"', '"macos"'], ['"x86_64"', '"little"']),
        ("Type: '#[cfg(target_endian = \"' → endian", '#[cfg(target_endian = "', 0, 24,
         ['"little"', '"big"'], ['"linux"']),
        ("Type: '#[cfg(debug_assertions = ' → bool", '#[cfg(debug_assertions = ', 0, 25,
         ["true", "false"], ['"linux"']),
        ("Type: '#[cfg(unix' → shows unix", "#[cfg(unix", 0, 10,
         ["unix"], ["target_os", "windows"]),
        ("Inside cfg(…) hides bare 'cfg'", "#[cfg(target_os = \"linux\")]", 0, 6,
         [], ["cfg"]),
        ("After comma: shows keys again", '#[cfg(target_os = "linux", ', 0, 29,
         ["target_arch", "unix", "all"]),
        ("Value prefix matching 'li'", '#[cfg(target_os = "li', 0, 21,
         ['"linux"'], ['"windows"', '"x86_64"']),
        ("Nested all(|) shows keys", "#[cfg(all(", 0, 10,
         ["target_os", "unix", "all"]),
        ("Value prefix matching empty string", '#[cfg(target_os = ', 0, 19,
         ['"linux"', '"windows"', '"macos"']),
        ("Key after all( shows nothing bad", "#[cfg(all(foo", 0, 13,
         [], []),  # just shouldn't crash
    ]

    print(f"Running {len(tests)} completion tests …\n")
    for args in tests:
        test(*args)

    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {len(tests)}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
