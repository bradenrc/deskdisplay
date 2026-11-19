"""Fetching pages the user points the start page at (feeds, site icons).

The start page lets anyone on the home network name a web address for the server to read, so
this refuses anything that is not a public web address: no localhost, no 192.168.x.x, no
other machines on the home network. Redirects are followed by hand so each hop is checked.
"""

from __future__ import annotations

import ipaddress
import socket
from urllib.parse import urljoin, urlparse

import requests

HEADERS = {
    "User-Agent": "Mozilla/5.0 (compatible; desk-companion/1.0; personal start page)",
    "Accept": "*/*",
}


class Refused(Exception):
    """The address is not one this server will fetch."""


def is_public(host: str) -> bool:
    try:
        found = socket.getaddrinfo(host, None)
    except OSError:
        return False
    return bool(found) and all(ipaddress.ip_address(i[4][0]).is_global for i in found)


def safe_get(
    url: str, max_bytes: int = 3_000_000, timeout: float = 10
) -> tuple[str, str, bytes]:
    """Returns (final address, content type, body). Raises on anything unexpected."""
    for _ in range(6):
        parts = urlparse(url)
        if parts.scheme not in ("http", "https") or not parts.hostname:
            raise Refused("Only http and https addresses can be read")
        if not is_public(parts.hostname):
            raise Refused("That address is not on the public internet")
        with requests.get(
            url, headers=HEADERS, timeout=timeout, allow_redirects=False, stream=True
        ) as resp:
            if resp.is_redirect or resp.is_permanent_redirect:
                url = urljoin(url, resp.headers.get("location", ""))
                continue
            resp.raise_for_status()
            body = resp.raw.read(max_bytes + 1, decode_content=True)
            if len(body) > max_bytes:
                raise Refused("That page is too large")
            return url, resp.headers.get("content-type", ""), body
    raise Refused("Too many redirects")
