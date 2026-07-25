#!/usr/bin/env python3
"""Test bitstruct hover in LSP features."""

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

    def close(self):
        self.proc.terminate()
        self.log.close()


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
        print("── Bitstruct Hover Tests ──")

        source = """\
use stdio::printf;

bitstruct Control : uint16 {
    enable  : 1;
    counter : 10;
}

struct Device {
    Control reg;
}

fn main() int32 {
    Control x;
    Device d;
    ret 0;
}
"""

        # Hover on 'Control' in bitstruct declaration (line 3: "bitstruct Control", col 10)
        test_hover("hover on bitstruct name 'Control'",
                   source, 2, 10,
                   "Control : uint16")

        # Hover on field 'enable' in bitstruct (line 4)
        test_hover("hover on bitstruct field 'enable'",
                   source, 3, 5,
                   "enable : 1")

        # Hover on field 'counter' in bitstruct (line 5)
        test_hover("hover on bitstruct field 'counter'",
                   source, 4, 5,
                   "counter : 10")

        # Hover on 'Control' used as struct field type (line 9: "    Control reg;")
        test_hover("hover on 'Control' as struct field type",
                   source, 8, 4,
                   "Control : uint16")

        # Hover on 'Control' used as local variable type (line 13)
        test_hover("hover on 'Control' as local variable type",
                   source, 12, 5,
                   "Control : uint16")

    finally:
        os.system("pkill -f 'lucis lsp' 2>/dev/null")

    print(f"\n{passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
