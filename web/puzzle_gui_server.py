#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parent
STATIC_DIR = ROOT / "puzzle_gui"
DEFAULT_TOOL = ROOT.parent / "bazel-bin" / "examples" / "puzzle_engine_tool"


def to_tool_request(payload: dict) -> str:
    setup = payload["setup"]
    lines = [
        f"target={setup['target']}",
        f"trump={setup['trump']}",
    ]

    if setup["startMode"] == "leader":
        lines.append(f"start=leader:{setup['leader']}")
    else:
        lines.append(
            f"start=opening:{setup['openingPlayer']}:{setup['openingCard']}"
        )

    lines.extend(
        [
            f"north={setup['north']}",
            f"east={setup['east']}",
            f"south={setup['south']}",
            f"west={setup['west']}",
            "moves=" + ",".join(payload.get("moves", [])),
        ]
    )
    return "\n".join(lines) + "\n"


class PuzzleGuiHandler(SimpleHTTPRequestHandler):
    tool_path = DEFAULT_TOOL

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_DIR), **kwargs)

    def do_GET(self) -> None:
        if self.path == "/":
            self.path = "/index.html"
        return super().do_GET()

    def do_POST(self) -> None:
        if self.path != "/api/analyse":
            self.send_error(HTTPStatus.NOT_FOUND, "Not found")
            return

        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)

        try:
            payload = json.loads(body.decode("utf-8"))
            tool_input = to_tool_request(payload)
        except Exception as exc:  # noqa: BLE001
            self._send_json(
                {"status": "error", "message": f"Bad request: {exc}"},
                status=HTTPStatus.BAD_REQUEST,
            )
            return

        try:
            completed = subprocess.run(
                [str(self.tool_path)],
                input=tool_input,
                text=True,
                capture_output=True,
                check=False,
            )
        except FileNotFoundError:
            self._send_json(
                {
                    "status": "error",
                    "message": f"Tool not found at {self.tool_path}. Build //examples:puzzle_engine_tool first.",
                },
                status=HTTPStatus.INTERNAL_SERVER_ERROR,
            )
            return

        raw = completed.stdout.strip() or completed.stderr.strip()
        try:
            response = json.loads(raw)
        except json.JSONDecodeError:
            response = {
                "status": "error",
                "message": raw or "Tool returned no JSON output",
            }

        http_status = HTTPStatus.OK
        if response.get("status") != "ok":
            http_status = HTTPStatus.BAD_REQUEST

        self._send_json(response, status=http_status)

    def _send_json(self, payload: dict, status: HTTPStatus) -> None:
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve the DDS puzzle GUI")
    parser.add_argument("--port", type=int, default=8123)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--tool", type=Path, default=DEFAULT_TOOL)
    args = parser.parse_args()

    PuzzleGuiHandler.tool_path = args.tool.resolve()
    server = ThreadingHTTPServer((args.host, args.port), PuzzleGuiHandler)
    print(f"Serving puzzle GUI on http://{args.host}:{args.port}")
    print(f"Using tool: {PuzzleGuiHandler.tool_path}")
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
