"""Google OAuth in the unmodified system default browser.

The official web client registers the exact localhost callback below.
Only our loopback listener receives its one-use code. No browser control,
extensions, profile files, passwords or cookies are accessed.
"""
import hmac
import html
import http.server
import ipaddress
import secrets
import select
import socket
import time
import urllib.parse
import webbrowser

GOOGLE_CLIENT_ID = '597053963349-htrrb3bejfe3s4dkeirsn02jcknl6pp7.apps.googleusercontent.com'
REDIRECT_URI = 'http://localhost:3000/home?signType=google'


class LoginCancelled(Exception):
    pass


class CallbackUnavailable(Exception):
    pass


def callback_code(url, redirect_uri, state):
    """Validate the complete callback, including provider, state and duplicates."""
    if len(url) > 16384:
        raise ValueError('oversized callback')
    actual, expected = urllib.parse.urlsplit(url), urllib.parse.urlsplit(redirect_uri)
    if (actual.scheme, actual.netloc, actual.path) != (expected.scheme, expected.netloc, expected.path):
        raise ValueError('unexpected callback origin or path')
    if actual.fragment or actual.username or actual.password:
        raise ValueError('invalid callback')
    fields = urllib.parse.parse_qs(actual.query, keep_blank_values=True, max_num_fields=24)
    fixed = urllib.parse.parse_qs(expected.query)
    if any(fields.get(key) != value for key, value in fixed.items()):
        raise ValueError('unexpected callback type')
    states = fields.get('state', [])
    if len(states) != 1 or not hmac.compare_digest(states[0], state):
        raise ValueError('state mismatch')
    if fields.get('error') == ['access_denied'] and 'code' not in fields:
        raise LoginCancelled()
    if 'error' in fields:
        raise ValueError('provider rejected authentication')
    codes = fields.get('code', [])
    if len(codes) != 1 or not 1 <= len(codes[0]) <= 4096:
        raise ValueError('invalid code')
    if any(ord(char) < 33 or ord(char) > 126 for char in codes[0]):
        raise ValueError('invalid code characters')
    return codes[0]


class LoopbackServer(http.server.HTTPServer):
    allow_reuse_address = False

    def server_bind(self):
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        else:
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        super().server_bind()


class BrowserSession:
    def __init__(self, region='global', timeout=300):
        if region not in ('global', 'cn'):
            raise ValueError('unsupported region')
        self.region = region
        self.deadline = time.monotonic() + timeout
        self.state = secrets.token_urlsafe(32)
        self.redirect_uri = REDIRECT_URI
        self.script_nonce = secrets.token_urlsafe(24)
        self.servers = []
        self.code = None
        self.cancelled = False
        self.claimed = False

    def authorization_url(self):
        return 'https://accounts.google.com/o/oauth2/v2/auth?' + urllib.parse.urlencode({
            'client_id': GOOGLE_CLIENT_ID, 'redirect_uri': self.redirect_uri,
            'response_type': 'code', 'scope': 'openid email profile',
            'access_type': 'offline', 'prompt': 'select_account', 'state': self.state,
        })

    def handler(self):
        session = self
        expected = urllib.parse.urlsplit(self.redirect_uri)

        class Handler(http.server.BaseHTTPRequestHandler):
            server_version = 'FlashStudio'
            sys_version = ''

            def setup(self):
                super().setup()
                self.connection.settimeout(2)

            def log_message(self, *args):
                pass  # Callback URLs contain one-use credentials.

            def reply(self, status, message, completed=False):
                script = (f'<script nonce="{session.script_nonce}">'
                          'history.replaceState(null,"","/complete")</script>') if completed else ''
                body = ('<!doctype html><html lang="ja"><meta charset="utf-8">'
                        '<meta name="viewport" content="width=device-width">'
                        '<title>FlashStudio ブラウザ認証</title>'
                        f'<h1>FlashStudio</h1><p>{html.escape(message)}</p>{script}</html>').encode('utf-8')
                self.send_response(status)
                self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Cache-Control', 'no-store')
                self.send_header('Referrer-Policy', 'no-referrer')
                self.send_header('X-Content-Type-Options', 'nosniff')
                self.send_header('Content-Security-Policy',
                    f"default-src 'none'; script-src 'nonce-{session.script_nonce}'; "
                    "frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                try:
                    if self.headers.get_all('Host') != [expected.netloc]:
                        return self.reply(400, '無効なリクエストです。')
                    if len(self.path) > 16384:
                        return self.reply(414, 'リクエストが長すぎます。')
                    url = urllib.parse.urlsplit(self.path)
                    if url.scheme or url.netloc or url.path != expected.path or url.fragment:
                        return self.reply(404, 'ページが見つかりません。')
                    if session.claimed or time.monotonic() >= session.deadline:
                        return self.reply(409, 'この認証リクエストは終了しました。')
                    try:
                        code = callback_code('http://' + expected.netloc + self.path,
                                             session.redirect_uri, session.state)
                    except LoginCancelled:
                        session.cancelled = True
                        session.claimed = True
                        return self.reply(200, 'ログインが中止されました。FlashStudioに戻ってください。', True)
                    except ValueError:
                        return self.reply(400, '認証リクエストが一致しません。アプリからもう一度お試しください。')
                    session.code = code
                    session.claimed = True
                    self.reply(200, '認証結果を受け取りました。FlashStudioでアカウントを確認しています。アプリに戻ってください。', True)
                except (ValueError, TypeError, OSError):
                    # A browser can close immediately after sending the code.
                    # The validated callback remains consumed exactly once.
                    pass

            def do_POST(self):
                self.reply(405, 'このリクエストは利用できません。')

        return Handler

    def start(self):
        expected = urllib.parse.urlsplit(self.redirect_uri)
        # Bind all resolved localhost addresses so IPv6 browsers cannot reach
        # another application. Never listen on a LAN or wildcard address.
        addresses = {(family, address[0]) for family, _, _, _, address in
                     socket.getaddrinfo('localhost', expected.port, type=socket.SOCK_STREAM)}
        if not addresses or any(not ipaddress.ip_address(address).is_loopback
                                for _, address in addresses):
            raise CallbackUnavailable()
        try:
            for family, address in sorted(addresses):
                server_type = type('CallbackServer', (LoopbackServer,), {'address_family': family})
                server = server_type((address, expected.port), self.handler())
                server.timeout = 0.2
                self.servers.append(server)
        except OSError:
            self.close()
            raise CallbackUnavailable() from None
        # Normal OS URL association: no executable, flags or browser profile.
        if not webbrowser.open(self.authorization_url(), new=2):
            raise OSError('could not open default browser')

    def wait_for_code(self):
        while not self.claimed and time.monotonic() < self.deadline:
            ready, _, _ = select.select(self.servers, [], [], 0.2)
            for server in ready:
                server.handle_request()
        if self.cancelled:
            raise LoginCancelled()
        if not self.claimed:
            raise TimeoutError()
        return self.code

    def close(self):
        for server in self.servers:
            server.server_close()
        self.servers.clear()
        self.code = None
        self.state = None

    def run(self, exchange):
        try:
            self.start()
            code = self.wait_for_code()
            # Refuse further callbacks before exchanging the one-use code.
            for server in self.servers:
                server.server_close()
            self.servers.clear()
            return exchange(code, self.redirect_uri, self.region)
        finally:
            self.close()
