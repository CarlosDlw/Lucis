#!/usr/bin/env python3
"""Test signature help for method calls, ensuring only the correct type's signature appears."""

import subprocess
import json
import sys
import os
import time
import select

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
ZTEST_DIR = os.path.join(os.path.dirname(__file__), "..", "ztests")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_stderr.log")

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
        self._init(root_uri)

    def _init(self, root_uri=None):
        self._send({
            "jsonrpc": "2.0", "id": 0, "method": "initialize",
            "params": {"processId": None,
                       "rootUri": root_uri or "file:///tmp",
                       "capabilities": {}}
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

    def signatureHelp(self, source, line, col, uri=None, timeout=10):
        uri = uri or "file:///tmp/_lsp_test.lc"
        self._send({
            "jsonrpc": "2.0", "id": 1, "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": uri, "languageId": "lucis", "version": 1, "text": source
                }
            }
        })
        time.sleep(0.5)
        # Drain the publishDiagnostics notification
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
        try:
            self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
            self._recv(timeout=2)
            self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()
        self.log.close()


def test(description, source, line, col,
         expect_count=None, expect_all=None, expect_none=None,
         root_uri=None, uri=None):
    global passed, failed
    expect_all = expect_all or []
    expect_none = expect_none or []

    client = LspClient(root_uri=root_uri)
    try:
        resp = client.signatureHelp(source, line, col, uri=uri, timeout=8)
    finally:
        client.close()

    signatures = []
    if resp and "result" in resp and resp["result"]:
        signatures = [s["label"] for s in resp["result"]["signatures"]]

    count_ok = True
    if expect_count is not None:
        count_ok = (len(signatures) == expect_count)

    all_ok = all(any(e in s for s in signatures) for e in expect_all) or not expect_all
    none_ok = all(all(e not in s for s in signatures) for e in expect_none) or not expect_none

    if count_ok and all_ok and none_ok:
        print(f"  OK   {description}")
        passed += 1
    else:
        print(f"  FAIL {description}")
        if not count_ok:
            print(f"       expected {expect_count} signatures, got {len(signatures)}")
        if not all_ok:
            missing = [e for e in expect_all if not any(e in s for s in signatures)]
            print(f"       missing: {missing}")
            print(f"       got: {signatures}")
        if not none_ok:
            unexpected = [e for e in expect_none if any(e in s for s in signatures)]
            print(f"       unexpected: {unexpected}")
            print(f"       got: {signatures}")
        failed += 1


def main():
    global passed, failed
    os.system("pkill -f 'lucis lsp' 2>/dev/null")
    time.sleep(0.2)
    try:
        os.remove(SERVER_LOG)
    except OSError:
        pass

    tests = []

    # Self-contained: struct + extend in same file (no project context)
    src1 = (
        "struct MyStruct { x: int32; }\n"
        "extend MyStruct { fn push(&self, v: int32) void { } }\n"
        "\n"
        "fn main() int32 {\n"
        "  MyStruct s;\n"
        "  s.push(42);\n"
        "  return 0;\n"
        "}\n"
    )
    tests.append((
        "local extend push shows only MyStruct (not Vec)",
        src1, 5, 9,
        1, ["(MyStruct) fn push"], ["Vec"],
    ))

    # Cross-file via project: Stack<int32>.push on indexed array element
    # Create temp file in ztests/ so project context discovers stdlib
    test2_src_file = os.path.join(ZTEST_DIR, "src", "_lsp_test_stack.lc")
    src2 = (
        "use container::Stack;\n"
        "\n"
        "fn main() int32 {\n"
        "  [10]Stack<int32> stackList;\n"
        "  stackList[0].push(10);\n"
        "  return 0;\n"
        "}\n"
    )
    with open(test2_src_file, "w") as f:
        f.write(src2)
    test2_uri = "file://" + test2_src_file
    test2_root = "file://" + ZTEST_DIR
    tests.append((
        "Stack<int32>.push shows Stack's push only (not ArrayList/Vec)",
        src2, 4, 20,
        1, ["(Stack) fn push"], ["ArrayList", "Vec"],
        test2_root, test2_uri,
    ))

    print(f"Running {len(tests)} signature help test(s) …\n")
    for args in tests:
        test(*args)

    # Cleanup temp files
    for fname in ["_lsp_test_stack.lc"]:
        fpath = os.path.join(ZTEST_DIR, "src", fname)
        try:
            os.remove(fpath)
        except OSError:
            pass

    print(f"\n{'='*50}")
    print(f"  {passed} passed, {failed} failed out of {len(tests)}")
    print(f"{'='*50}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
