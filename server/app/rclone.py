"""Google Drive through rclone.

rclone ships with its own registered Google app, so there is no Google Cloud project to
create. Sign-in happens on Google's pages in the browser; access is read-only Drive.

How the sign-in is wired: `rclone authorize` runs a tiny web server on 127.0.0.1:53682
inside the container and Google sends the browser back to that address. Two ways to finish:
  * the browser can reach port 53682 (same machine, port published): it completes by itself;
  * otherwise the browser shows "can't reach this page" and the user pastes that address
    into the setup page, which replays it to rclone from inside the container.
"""

from __future__ import annotations

import base64
import json
import logging
import os
import re
import socket
import subprocess
import threading
import time
from pathlib import Path
from urllib.parse import urlparse

import requests

log = logging.getLogger("deskpal.rclone")

REMOTE = "gdrive"
AUTH_PORT = 53682
AUTH_URL_RE = re.compile(rf"http://127\.0\.0\.1:{AUTH_PORT}/auth\?state=[A-Za-z0-9_-]+")
AUTH_TIMEOUT = 10 * 60
IMAGE_GLOB = "*.{jpg,jpeg,png,heic,heif,webp}"


class RcloneError(Exception):
    pass


def extract_token(output: str) -> str | None:
    """Pull the token JSON out of `rclone authorize` output (plain or base64-wrapped)."""
    match = re.search(r"--->\s*(.+?)\s*<---", output, re.DOTALL)
    if not match:
        return None
    blob = match.group(1).strip()
    if not blob.startswith("{"):
        try:
            blob = base64.b64decode(blob + "=" * (-len(blob) % 4)).decode()
        except Exception:
            return None
    try:
        data = json.loads(blob)
    except ValueError:
        return None
    if isinstance(data.get("token"), str):  # wrapped form: {"token": "<json string>"}
        blob = data["token"]
        data = json.loads(blob)
    return blob if data.get("refresh_token") or data.get("access_token") else None


class Rclone:
    def __init__(self, data_dir: Path, on_connected=None):
        self.dir = data_dir / "rclone"
        self.dir.mkdir(parents=True, exist_ok=True)
        self.conf = self.dir / "rclone.conf"
        self.on_connected = on_connected
        self._lock = threading.Lock()
        self._proc: subprocess.Popen | None = None
        self._output: list[str] = []
        self._started = 0.0
        self._error = ""
        self._forwarder_started = False

    # ---- state -------------------------------------------------------------------

    def _env(self) -> dict:
        return {
            **os.environ,
            "RCLONE_CONFIG": str(self.conf),
            "RCLONE_DRIVE_SCOPE": "drive.readonly",
        }

    def available(self) -> bool:
        try:
            subprocess.run(
                ["rclone", "version"], capture_output=True, timeout=10, check=True
            )
            return True
        except Exception:
            return False

    def connected(self) -> bool:
        return self.conf.exists() and "token" in self.conf.read_text()

    def signing_in(self) -> bool:
        with self._lock:
            return self._proc is not None and self._proc.poll() is None

    def last_error(self) -> str:
        return self._error

    def disconnect(self) -> None:
        self._stop_auth()
        self.conf.unlink(missing_ok=True)

    # ---- sign-in -----------------------------------------------------------------

    def start_auth(self) -> str:
        """Start a sign-in attempt and return the Google address to send the browser to."""
        self._stop_auth()
        self._error = ""
        try:
            proc = subprocess.Popen(
                ["rclone", "authorize", "drive", "--auth-no-open-browser"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                env=self._env(),
            )
        except FileNotFoundError as exc:
            raise RcloneError("rclone is not installed in this container.") from exc
        with self._lock:
            self._proc, self._output, self._started = proc, [], time.time()
        threading.Thread(target=self._watch, args=(proc,), daemon=True).start()

        local_url = None
        deadline = time.time() + 15
        while time.time() < deadline and proc.poll() is None:
            found = AUTH_URL_RE.search("".join(self._output))
            if found:
                local_url = found.group(0)
                break
            time.sleep(0.1)
        if not local_url:
            self._stop_auth()
            raise RcloneError(
                "rclone did not start its sign-in. Check the container logs."
            )
        self._start_forwarder()
        for _ in range(20):  # the little web server may need a moment to listen
            try:
                resp = requests.get(local_url, allow_redirects=False, timeout=5)
                google_url = resp.headers.get("Location", "")
                if google_url.startswith("https://accounts.google.com/"):
                    return google_url
            except requests.RequestException:
                pass
            time.sleep(0.25)
        self._stop_auth()
        raise RcloneError("Could not get the Google sign-in address from rclone.")

    def complete_from_pasted(self, pasted: str) -> None:
        """Replay the address the browser ended on (http://127.0.0.1:53682/?state=…&code=…)."""
        if not self.signing_in():
            raise RcloneError("That sign-in attempt has expired. Start again.")
        parsed = urlparse(pasted.strip())
        if "code=" not in parsed.query or "state=" not in parsed.query:
            raise RcloneError("Paste the whole address from the browser's address bar.")
        try:
            requests.get(
                f"http://127.0.0.1:{AUTH_PORT}{parsed.path or '/'}?{parsed.query}",
                timeout=60,
            )
        except requests.RequestException as exc:
            raise RcloneError(
                "rclone did not accept that address. Start again."
            ) from exc
        deadline = time.time() + 30
        while time.time() < deadline:
            if self.connected():
                return
            if not self.signing_in():
                break
            time.sleep(0.2)
        time.sleep(0.3)
        if not self.connected():
            raise RcloneError(
                self._error or "Google did not confirm the sign-in. Start again."
            )

    def _watch(self, proc: subprocess.Popen) -> None:
        timer = threading.Timer(AUTH_TIMEOUT, proc.kill)
        timer.daemon = True
        timer.start()
        try:
            for line in proc.stdout:  # type: ignore[union-attr]
                self._output.append(line)
            proc.wait()
        finally:
            timer.cancel()
        token = extract_token("".join(self._output))
        with self._lock:
            mine = self._proc is proc
        if not mine:
            return
        if token:
            self.conf.write_text(
                f"[{REMOTE}]\ntype = drive\nscope = drive.readonly\ntoken = {token}\n"
            )
            self.conf.chmod(0o600)
            log.info("Google Drive connected")
            if self.on_connected:
                self.on_connected()
        else:
            self._error = "The sign-in did not finish. Start again."

    def _stop_auth(self) -> None:
        with self._lock:
            proc, self._proc = self._proc, None
        if proc and proc.poll() is None:
            proc.kill()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass

    def _start_forwarder(self) -> None:
        """Let a published port 53682 reach rclone, which only listens on loopback."""
        if self._forwarder_started:
            return
        try:
            address = socket.gethostbyname(socket.gethostname())
        except OSError:
            return
        if address.startswith("127."):
            return  # not in a container: the browser can reach rclone directly
        try:
            server = socket.create_server((address, AUTH_PORT))
        except OSError as exc:
            log.info("no port forwarder (%s); the paste method still works", exc)
            return
        self._forwarder_started = True

        def pipe(src: socket.socket, dst: socket.socket) -> None:
            try:
                while chunk := src.recv(65536):
                    dst.sendall(chunk)
            except OSError:
                pass
            finally:
                for sock in (src, dst):
                    try:
                        sock.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass

        def serve() -> None:
            while True:
                client, _ = server.accept()
                try:
                    upstream = socket.create_connection(
                        ("127.0.0.1", AUTH_PORT), timeout=5
                    )
                except OSError:
                    client.close()
                    continue
                threading.Thread(
                    target=pipe, args=(client, upstream), daemon=True
                ).start()
                threading.Thread(
                    target=pipe, args=(upstream, client), daemon=True
                ).start()

        threading.Thread(target=serve, name="auth-forwarder", daemon=True).start()

    # ---- Drive -------------------------------------------------------------------

    def _run(self, args: list[str], timeout: int) -> str:
        try:
            done = subprocess.run(
                ["rclone", *args],
                capture_output=True,
                text=True,
                timeout=timeout,
                env=self._env(),
            )
        except subprocess.TimeoutExpired as exc:
            raise RcloneError("Google Drive took too long to respond.") from exc
        if done.returncode != 0:
            lines = [line for line in done.stderr.strip().splitlines() if line.strip()]
            detail = lines[-1][-200:] if lines else "unknown error"
            if "invalid_grant" in done.stderr or "token" in detail.lower():
                detail = (
                    "Google Drive sign-in has expired. Reconnect on the setup page."
                )
            raise RcloneError(detail)
        return done.stdout

    @staticmethod
    def _root(folder_id: str) -> list[str]:
        return (
            [] if folder_id in ("", "root") else ["--drive-root-folder-id", folder_id]
        )

    def list_images(self, folder_id: str) -> list[dict]:
        """Every image under the folder: [{id, path}]. Names only; nothing is downloaded."""
        out = self._run(
            [
                "lsjson",
                f"{REMOTE}:",
                "-R",
                "--files-only",
                "--fast-list",
                "--no-modtime",
                "--no-mimetype",
                "--ignore-case",
                "--include",
                IMAGE_GLOB,
                *self._root(folder_id),
            ],
            timeout=30 * 60,
        )
        return [
            {"id": item.get("ID") or item["Path"], "path": item["Path"]}
            for item in json.loads(out or "[]")
        ]

    def copy(self, folder_id: str, paths: list[str], dest: Path) -> None:
        """Download the given files (paths relative to the folder) into dest, same layout."""
        listing = dest / ".files-from"
        listing.write_text("\n".join(paths) + "\n")
        try:
            self._run(
                [
                    "copy",
                    f"{REMOTE}:",
                    str(dest),
                    "--files-from-raw",
                    str(listing),
                    "--no-traverse",
                    "--transfers",
                    "4",
                    *self._root(folder_id),
                ],
                timeout=30 * 60,
            )
        finally:
            listing.unlink(missing_ok=True)
