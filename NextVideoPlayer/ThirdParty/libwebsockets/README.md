# libwebsockets build input

`dependency.json` pins the upstream source revision and archive SHA256.
`setup_websocket.ps1` verifies the archive and builds a static library using
Visual Studio 2026. Source and build products are cached in `artifacts/`.

Pinned main revision: `7aed761cbe5429db8493523959e7a5108d7eb77a` (2026-11-09).
The upstream GitHub mirror did not expose a v5.1-stable branch when pinned;
The downloaded revision is pinned to this exact main commit. Known security-fix
ancestors were reviewed during selection. Update the revision and SHA256
together after reviewing new upstream security fixes.

Only the HTTP/1 + WebSocket server is needed. TLS, compression, HTTP/2,
HTTP/3/QUIC, MQTT, clients, image decoders, CGI, plugins and optional service
modules are disabled. Windows system libraries provide the runtime dependencies
for this configuration. License notices are in `LICENSE.txt` and are included
in the distribution packages.

Source: https://github.com/warmcat/libwebsockets
