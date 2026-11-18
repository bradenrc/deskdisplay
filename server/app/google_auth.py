"""Google sign-in for the server (OAuth 2.0, read-only scopes).

The password and the two-step verification prompt are handled entirely on Google's own
pages in the browser. This server only ever receives a revocable token limited to
read-only Calendar and read-only Drive.
"""

from __future__ import annotations

import json
import os
import threading
import time
from pathlib import Path
from urllib.parse import parse_qs, urlparse

# Google may return previously granted scopes alongside the ones requested.
os.environ.setdefault("OAUTHLIB_RELAX_TOKEN_SCOPE", "1")

from google.auth.transport.requests import AuthorizedSession
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import Flow

SCOPES = [
    "https://www.googleapis.com/auth/calendar.readonly",
    "https://www.googleapis.com/auth/drive.readonly",
]
PENDING_SECONDS = 15 * 60


class AuthError(Exception):
    pass


class GoogleAuth:
    def __init__(self, data_dir: Path, config_dir: Path | None):
        self.token_path = data_dir / "google_token.json"
        self._client_candidates = [data_dir / "google_client.json"]
        if config_dir:
            self._client_candidates.append(config_dir / "client_secret.json")
        self._pending: dict[str, tuple[Flow, float]] = {}
        self._lock = threading.Lock()

    # ---- client (the "app" registered in Google Cloud) ---------------------------

    def client_path(self) -> Path | None:
        for path in self._client_candidates:
            if path.exists():
                return path
        return None

    def save_client(self, text: str) -> None:
        try:
            data = json.loads(text)
        except ValueError as exc:
            raise AuthError(
                "That is not valid JSON. Paste the whole downloaded file."
            ) from exc
        section = (
            data.get("web") or data.get("installed") if isinstance(data, dict) else None
        )
        if (
            not section
            or not section.get("client_id")
            or not section.get("client_secret")
        ):
            raise AuthError("This does not look like a Google OAuth client file.")
        path = self._client_candidates[0]
        path.write_text(json.dumps(data))
        path.chmod(0o600)

    # ---- token -------------------------------------------------------------------

    def connected(self) -> bool:
        return self.token_path.exists()

    def session(self) -> AuthorizedSession | None:
        if not self.token_path.exists():
            return None
        creds = Credentials.from_authorized_user_file(str(self.token_path), SCOPES)
        return AuthorizedSession(creds)

    def disconnect(self) -> None:
        self.token_path.unlink(missing_ok=True)

    # ---- sign-in flow ------------------------------------------------------------

    def start(self, redirect_uri: str) -> str:
        client = self.client_path()
        if not client:
            raise AuthError("Add the Google client file first.")
        flow = Flow.from_client_secrets_file(
            str(client), scopes=SCOPES, redirect_uri=redirect_uri
        )
        url, state = flow.authorization_url(access_type="offline", prompt="consent")
        now = time.time()
        with self._lock:
            self._pending = {
                key: value
                for key, value in self._pending.items()
                if now - value[1] < PENDING_SECONDS
            }
            self._pending[state] = (flow, now)
        return url

    def finish(self, state: str, code: str) -> None:
        with self._lock:
            pending = self._pending.pop(state, None)
        if not pending or time.time() - pending[1] > PENDING_SECONDS:
            raise AuthError("That sign-in attempt has expired. Start again.")
        flow = pending[0]
        try:
            flow.fetch_token(code=code)
        except Exception as exc:
            raise AuthError(f"Google rejected the sign-in: {exc}") from exc
        creds = flow.credentials
        if not creds.refresh_token:
            raise AuthError(
                "Google did not issue a long-lived token. Remove this app at "
                "myaccount.google.com/permissions and sign in again."
            )
        self.token_path.write_text(creds.to_json())
        self.token_path.chmod(0o600)

    def finish_from_pasted(self, pasted: str) -> None:
        """Accept the full address the browser ended on (…/auth/callback?state=…&code=…)."""
        query = parse_qs(urlparse(pasted.strip()).query)
        if "error" in query:
            raise AuthError(f"Google reported: {query['error'][0]}")
        if "code" not in query or "state" not in query:
            raise AuthError("Paste the whole address from the browser's address bar.")
        self.finish(query["state"][0], query["code"][0])
