"""Google login in the normal system default browser, without an extension.

Only the parent's anonymous pipe receives successful Flashforge tokens.
Never run this helper directly in a terminal for a real account.
"""
import argparse
import json
import sys
import time
import urllib.error
import urllib.request

from loopback_browser import BrowserSession, LoginCancelled, CallbackUnavailable

CLIENT_ID = '9157eef8-14fe-4791-8b70-3bef0d4e0070'


class AuthenticationRejected(Exception):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def exchange_provider_code(code, redirect_uri, region):
    host = 'auth.voxelshare.cn' if region == 'cn' else 'auth.voxelshare.com'
    body = json.dumps({'type': 'google', 'auth_code': code,
                       'client_id': CLIENT_ID, 'redirect_uri': redirect_uri}).encode('utf-8')
    request = urllib.request.Request('https://' + host + '/oauth/token/authCode', data=body,
        headers={'Content-Type': 'application/json', 'Accept': 'application/json'}, method='POST')
    opener = urllib.request.build_opener(NoRedirect(), urllib.request.ProxyHandler({}))
    with opener.open(request, timeout=20) as response:
        raw = response.read(32769)
    if len(raw) > 32768:
        raise AuthenticationRejected()
    data = json.loads(raw)
    if isinstance(data, dict) and 'code' in data:
        if data['code'] != 0:
            raise AuthenticationRejected()
        data = data.get('data')
    if not isinstance(data, dict):
        raise AuthenticationRejected()
    access = data.get('access_token', data.get('accessToken'))
    refresh = data.get('refresh_token', data.get('refreshToken'))
    expires = data.get('expires_in', data.get('expiresIn'))
    for token in (access, refresh):
        if not isinstance(token, str) or not 1 <= len(token) <= 8192 or any(ord(c) < 33 for c in token):
            raise AuthenticationRejected()
    if type(expires) is not int or not 0 < expires <= 315360000:
        raise AuthenticationRejected()
    return {'access_token': access, 'refresh_token': refresh,
            'expires_in': expires, 'start_time': int(time.time())}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--region', choices=('global', 'cn'), default='global')
    args = parser.parse_args()
    try:
        result = BrowserSession(args.region).run(exchange_provider_code)
        sys.stdout.write(json.dumps(result, separators=(',', ':')))
        sys.stdout.flush()
        return 0
    except LoginCancelled:
        return 5
    except CallbackUnavailable:
        return 7
    except TimeoutError:
        return 3
    except (urllib.error.HTTPError, AuthenticationRejected):
        return 6
    except Exception:
        # Errors and provider payloads must never reach stdout, stderr or logs.
        return 4


if __name__ == '__main__':
    sys.exit(main())
