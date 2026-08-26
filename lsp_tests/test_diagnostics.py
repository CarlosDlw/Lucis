#!/usr/bin/env python3
"""LSP diagnostics hardening: publishDiagnostics content + didOpen/didChange/didSave.

Covers exactly what the other tests do not: what the server publishes in
publishDiagnostics, and that errors clear/reappear as the document changes
while typing. A single LSP session for every scenario (avoids the
startup/rebuild cost of the older tests — no sleeps, event-driven reads).
"""

import json
import os
import select
import subprocess
import sys
import tempfile
import time

LUCIS_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "lucis")
SERVER_LOG = os.path.join(os.path.dirname(__file__), "lsp_diag_stderr.log")

passed = 0
failed = 0


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"PASS: {name}")
    else:
        failed += 1
        print(f"FAIL: {name} {detail}")


GOOD = "fn main() int32 {\n    ret 0;\n}\n"
BAD_TYPE = 'fn main() int32 {\n    int32 x = "oops";\n    ret 0;\n}\n'
BAD_SYNTAX = "fn main() int32 {\n    ret ;;\n}\n"


class LspSession:
    def __init__(self, root, uri):
        self.log = open(SERVER_LOG, "ab")
        self.proc = subprocess.Popen(
            [LUCIS_BIN, "lsp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.log, text=True, bufsize=0,
        )
        self._send({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                    "params": {"processId": None, "rootUri": "file://" + root,
                               "capabilities": {}}})
        self._rev(id=0)          # initialize response
        self._send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        self.uri = uri

    def _send(self, obj):
        body = json.dumps(obj, ensure_ascii=False)
        self.proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
        self.proc.stdin.flush()

    def _frame(self, timeout):
        # Read one full LSP frame (headers + body). Returns a dict or None.
        raw = self.proc.stdout.buffer
        deadline = time.monotonic() + timeout
        content_len = 0
        while time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
            if not r:
                return None
            line = b""
            while time.monotonic() < deadline:
                r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
                if not r:
                    return None
                ch = raw.read(1)
                if ch == b"":
                    return None   # EOF — server went away
                if ch == b"\n":
                    break
                line += ch
            hdr = line.decode(errors="replace").strip()
            if hdr.startswith("Content-Length:"):
                content_len = int(hdr.split(":")[1].strip())
            elif hdr == "":
                break   # end of headers — read the body next
        # Read the body
        body = b""
        while len(body) < content_len and time.monotonic() < deadline:
            r, _, _ = select.select([raw], [], [], max(0.05, deadline - time.monotonic()))
            if not r:
                return None
            chunk = raw.read(content_len - len(body))
            if chunk == b"":
                return None   # EOF — server went away
            body += chunk
        return json.loads(body.decode("utf-8"))

    def _rev(self, id=None):
        """Read frames until a message with the given id arrives."""
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            msg = self._frame(deadline - time.monotonic())
            if msg is None:
                return None
            if msg.get("id") == id:
                return msg
        return None

    def did_open(self, text):
        self._send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
                    "params": {"textDocument": {"uri": self.uri, "text": text}}})

    def did_change(self, text):
        self._send({"jsonrpc": "2.0", "method": "textDocument/didChange",
                    "params": {"textDocument": {"uri": self.uri},
                               "contentChanges": [{"text": text}]}})

    def did_save(self, text):
        self._send({"jsonrpc": "2.0", "method": "textDocument/didSave",
                    "params": {"textDocument": {"uri": self.uri}, "text": text}})

    def publish(self, timeout=10):
        """Read frames until the next publishDiagnostics for this uri."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self._frame(deadline - time.monotonic())
            if msg is None:
                return None
            if (msg.get("method") == "textDocument/publishDiagnostics"
                    and msg.get("params", {}).get("uri") == self.uri):
                return msg["params"].get("diagnostics", [])
        return None

    def close(self):
        self._send({"jsonrpc": "2.0", "id": 99, "method": "shutdown", "params": {}})
        self._rev(id=99)
        self._send({"jsonrpc": "2.0", "method": "exit", "params": {}})
        self.proc.stdin.flush()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.log.close()


def messages(diags):
    return [d.get("message", "") for d in (diags or [])]


def main():
    # A real project fixture: semantic diagnostics only run when the
    # ProjectContext is valid (lucis.yaml + sources), matching the LSP design.
    tmp = tempfile.mkdtemp(prefix="lucis_lsp_diag_")
    src_dir = os.path.join(tmp, "src")
    os.makedirs(src_dir)
    with open(os.path.join(tmp, "lucis.yaml"), "w") as f:
        f.write("name: diagtest\n"
                "version: 0.0.1\n"
                "binary: diagtest\n"
                "out_dir: build\n"
                "source:\n"
                "  - src/\n")
    uri = "file://" + os.path.join(src_dir, "main.lc")

    s = LspSession(tmp, uri)
    try:
        # didOpen with a type error -> publish carries the error.
        s.did_open(BAD_TYPE)
        diags = s.publish()
        check("didOpen publishes a type error",
              any("type mismatch" in m for m in messages(diags)),
              f"got={messages(diags)}")

        # didChange fixing it -> errors clear (the typing cycle).
        s.did_change(GOOD)
        diags = s.publish()
        check("didChange with a fix clears errors",
              messages(diags) == [],
              f"got={messages(diags)}")

        # didChange with invalid syntax -> diagnostics come back.
        s.did_change(BAD_SYNTAX)
        diags = s.publish()
        check("didChange with invalid syntax publishes diagnostics",
              len(diags or []) >= 1,
              f"got={messages(diags)}")

        # didChange back to good text -> clears again.
        s.did_change(GOOD)
        diags = s.publish()
        check("didChange recovered publishes clean",
              messages(diags) == [],
              f"got={messages(diags)}")

        # didSave with an error -> publish the error (persisted state).
        s.did_save(BAD_TYPE)
        diags = s.publish()
        check("didSave publishes the error of the saved file",
              any("type mismatch" in m for m in messages(diags)),
              f"got={messages(diags)}")

        # didSave with good text -> clears.
        s.did_save(GOOD)
        diags = s.publish()
        check("didSave clean publishes no errors",
              messages(diags) == [],
              f"got={messages(diags)}")
    finally:
        s.close()

    print(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())