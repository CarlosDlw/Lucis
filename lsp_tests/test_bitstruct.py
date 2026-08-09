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
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self._recv_one(deadline - time.monotonic())
            if msg is None:
                return None
            if "id" in msg:
                return msg
        return None

    def _recv_one(self, timeout=5):
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

    def completions(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
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

    def signatureHelp(self, source, line, col, timeout=10):
        uri = "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "lucis", "version": 1, "text": source}}
        })
        time.sleep(0.2)
        self._recv(timeout=3)
        self._send({
            "jsonrpc": "2.0", "id": 2, "method": "textDocument/signatureHelp",
            "params": {
                "textDocument": {"uri": uri},
                "position": {"line": line, "character": col}
            }
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


def test_completion(description, source, line, col, expect_label):
    global passed, failed
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
    ok = expect_label in labels
    if ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        print(f"       expected '{expect_label}' in completions: {labels[:20]}")
        failed += 1


def test_signature_help(description, source, line, col, expect_contains):
    global passed, failed
    client = LspClient()
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
        print(f"       expected '{expect_contains}' in signatures: {labels[:10]}")
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

        print("── Bitstruct Completion Tests ──")

        # Completion for 'bit' keyword (should suggest 'bitstruct')
        test_completion("completion 'bit' suggests 'bitstruct'",
                        "bit", 0, 3,
                        "bitstruct")

        # Completion for type name 'C' after bitstruct definition in same file
        test_completion("completion 'Con' suggests 'Control'",
                        "bitstruct Control : uint16 {}\nfn main() {\n  Con\n}\n",
                        2, 2,
                        "Control")

        # Completion for bitstruct field after dot
        # Source line 1 (0-indexed): fn main() int32 { Control x; x.
        #                                            ^31 is after the dot
        src_bs = ("bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
                  "fn main() int32 { Control x; x.\nret 0; }\n")
        test_completion("completion 'x.' suggests field 'enable'",
                        src_bs, 1, 31,
                        "enable")
        test_completion("completion 'x.' suggests field 'counter'",
                        src_bs, 1, 31,
                        "counter")
        test_completion("completion 'x.' suggests method 'raw'",
                        src_bs, 1, 31,
                        "raw")
        test_completion("completion 'x.' suggests method 'clearAll'",
                        src_bs, 1, 31,
                        "clearAll")
        test_completion("completion 'x.' suggests method 'setAll'",
                        src_bs, 1, 31,
                        "setAll")

        # Hover on bitstruct field access expression: x.enable
        # Line 1: fn main() int32 { Control x; ret x.enable; }
        #                                        enable starts at col 35
        test_hover("hover on field 'enable' in 'x.enable'",
                    "bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
                    "fn main() int32 { Control x; ret x.enable; }",
                    1, 35,
                    "enable : 1")

        # Hover on bitstruct method call: x.raw()
        # Line 1: fn main() int32 { Control x; ret x.raw(); }
        #                                                   raw starts at col 37
        src_mh = ("bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
                  "fn main() int32 { Control x; ret x.raw(); }")
        test_hover("hover on method 'raw' in 'x.raw()'",
                    src_mh, 1, 37,
                    "Control method) uint16 raw()")

        print("── Bitstruct Signature Help Tests ──")

        # Signature help for bitstruct method: x.raw(|)
        # Line 1: fn main() int32 { Control x; ret x.raw(); }
        #                                                   '(' at col 38, cursor at col 39
        src_sh = ("bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
                  "fn main() int32 { Control x; ret x.raw(); }")
        test_signature_help("signature help for 'x.raw('",
                            src_sh, 1, 39,
                            "raw()")

        print("── Bitstruct Chained Completion/Hover/Signature Tests ──")

        # All on one line so base variable is visible to completion analyzer
        src_chain_oneline = (
            "struct Device { Control reg; }\n"
            "bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
            "fn main() int32 { Device atc; atc.reg.\nret 0; }\n"
        )
        # Line 2: fn main() int32 { Device atc; atc.reg.
        #   atc starts at col 25, '.' at 33, reg at 34, '.' at 37
        #   cursor after last '.' = col 38
        test_completion("completion 'atc.reg.' suggests 'enable'",
                        src_chain_oneline, 2, 38, "enable")
        test_completion("completion 'atc.reg.' suggests 'counter'",
                        src_chain_oneline, 2, 38, "counter")
        test_completion("completion 'atc.reg.' suggests 'raw'",
                        src_chain_oneline, 2, 38, "raw")

        # Hover on 'counter' in 'atc.reg.counter' (single line)
        src_chain_h = (
            "struct Device { Control reg; }\n"
            "bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
            "fn main() int32 { Device atc; ret atc.reg.counter; }\n"
        )
        # fn main() int32 { Device atc; ret atc.reg.counter; }
        # counter starts at col 42
        test_hover("hover on 'counter' in 'atc.reg.counter'",
                   src_chain_h, 2, 44, "counter : 10")
        # Verify we can hover on 'atc' too
        test_hover("hover on 'atc' in chained source",
                   src_chain_h, 2, 25, "Device")

        # Hover on method via chain: atc.reg.raw()
        src_chain_mh = (
            "struct Device { Control reg; }\n"
            "bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
            "fn main() int32 { Device atc; ret atc.reg.raw(); }\n"
        )
        #                   fn main() int32 { Device atc; ret atc.reg.raw(); }
        #                    'raw' starts at col 42 ('r' at 42, 'a' at 43, 'w' at 44)
        #                    '(' at col 45
        test_hover("hover on 'raw' in 'atc.reg.raw()'",
                   src_chain_mh, 2, 43,
                   "Control method) uint16 raw()")

        # Signature help at atc.reg.raw(|)
        # '(' is at col 45, cursor after paren = col 46
        test_signature_help("signature help for 'atc.reg.raw('",
                            src_chain_mh, 2, 46, "raw()")

        # Hover on 'raw' variable from 'auto raw = reg.raw()' — should be uint16
        src_rawvar = ("bitstruct Control : uint16 { enable : 1; }\n"
                      "fn main() int32 { Control reg; auto raw = reg.raw(); ret 0; }")
        #           fn main() int32 { Control reg; auto raw = reg.raw(); ret 0; }
        #                                             'raw' starts at col 36
        test_hover("hover on 'raw' variable from 'reg.raw()'",
                   src_rawvar, 1, 37, "uint16")

        # Completion after 'raw.' on the auto variable — should show integer methods
        src_rawcomp = ("bitstruct Control : uint16 { enable : 1; }\n"
                       "fn main() int32 { Control reg; auto raw = reg.raw(); raw.\nret 0; }")
        #                    fn main() int32 { Control reg; auto raw = reg.raw(); raw.
        #                                                                          '.' at col 56, cursor at 57
        test_completion("completion 'raw.' suggests 'toString'",
                        src_rawcomp, 1, 57, "toString")
        test_completion("completion 'raw.' suggests 'toFloat'",
                        src_rawcomp, 1, 57, "toFloat")

        # Auto backing type: bitstruct Control : auto { enable : 1; counter : 10; }
        # 11 bits → uint16 backing
        src_auto = ("bitstruct Control : auto { enable : 1; counter : 10; }\n"
                    "fn main() int32 { Control x; ret x.raw(); }")
        test_hover("hover on 'raw()' with auto backing type",
                   src_auto, 1, 35,
                   "Control method) uint16 raw()")

        # Hover on field in assignment context: atc.reg.counter = 42
        # fn main() int32 { Device atc; atc.reg.counter = 42; }
        src_assign = ("struct Device { Control reg; }\n"
                      "bitstruct Control : uint16 { enable : 1; counter : 10; }\n"
                      "fn main() int32 { Device atc; atc.reg.counter = 42; }")
        #                   fn main() int32 { Device atc; atc.reg.counter = 42; }
        #                    'counter' starts at col 41
        test_hover("hover on 'counter' in 'atc.reg.counter = 42'",
                   src_assign, 2, 43, "counter")
    finally:
        os.system("pkill -f 'lucis lsp' 2>/dev/null")

    print(f"\n{passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
