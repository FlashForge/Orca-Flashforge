import contextlib
import io
import json
import http.client
import socket
import threading
import time
import unittest
from unittest.mock import patch
import urllib.parse
import broker
import loopback_browser as native_browser

CALLBACK='http://localhost:3000/home?signType=google'

class CallbackTests(unittest.TestCase):
    def url(self, **changes):
        values={'signType':'google','state':'expected-state','code':'synthetic-code'}
        values.update(changes)
        return 'http://localhost:3000/home?'+urllib.parse.urlencode(values,doseq=True)
    def test_valid_code(self):
        self.assertEqual(native_browser.callback_code(self.url(),CALLBACK,'expected-state'),'synthetic-code')
    def test_state_required(self):
        for state in ['', 'wrong', ['expected-state','expected-state']]:
            with self.subTest(state=state),self.assertRaises(ValueError):
                native_browser.callback_code(self.url(state=state),CALLBACK,'expected-state')
    def test_code_validation(self):
        for code in ['', 'a'*4097, 'a\nb', ['one','two']]:
            with self.subTest(code=str(code)[:20]),self.assertRaises(ValueError):
                native_browser.callback_code(self.url(code=code),CALLBACK,'expected-state')
    def test_exact_origin_path_and_protocol(self):
        url=self.url()
        for wrong in [url.replace('http:','https:'),url.replace('localhost:3000','localhost.attacker.invalid:3000'),url.replace('/home?','/other?'),url.replace('localhost:3000','user@localhost:3000'),url.replace(':3000',':444'),url+'#unexpected']:
            with self.subTest(url=wrong),self.assertRaises(ValueError):
                native_browser.callback_code(wrong,CALLBACK,'expected-state')
    def test_wrong_provider_rejected(self):
        with self.assertRaises(ValueError):native_browser.callback_code(self.url(signType='facebook'),CALLBACK,'expected-state')
    def test_user_denial_is_cancellation(self):
        with self.assertRaises(native_browser.LoginCancelled):
            native_browser.callback_code(self.url(error='access_denied',code=[]),CALLBACK,'expected-state')
    def test_error_requires_correct_state(self):
        with self.assertRaises(ValueError):
            native_browser.callback_code(self.url(error='access_denied',state='wrong'),CALLBACK,'expected-state')
    def test_callback_length_limited(self):
        with self.assertRaises(ValueError):native_browser.callback_code(self.url(extra='a'*17000),CALLBACK,'expected-state')
    def test_session_randomness(self):
        a=native_browser.BrowserSession();b=native_browser.BrowserSession()
        self.assertNotEqual(a.state,b.state)
    def test_authorization_uses_registered_callback(self):
        s=native_browser.BrowserSession()
        fields=urllib.parse.parse_qs(urllib.parse.urlsplit(s.authorization_url()).query)
        self.assertEqual(fields['redirect_uri'],[CALLBACK])
        self.assertEqual(fields['state'],[s.state])
        self.assertEqual(fields['scope'],['openid email profile'])
    def test_browser_control_is_not_used(self):
        self.assertFalse(hasattr(native_browser.BrowserSession, 'send'))
        self.assertFalse(hasattr(native_browser, 'subprocess'))
        self.assertFalse(hasattr(native_browser, 'websocket'))

class ExchangeTests(unittest.TestCase):
    def exchange(self, response):
        captured=[]
        class Reply:
            def __enter__(self):return self
            def __exit__(self,*args):pass
            def read(self,size):return json.dumps(response).encode()
        class Opener:
            def open(self,request,timeout):captured.append(request);return Reply()
        with patch.object(broker.urllib.request,'build_opener',return_value=Opener()):
            result=broker.exchange_provider_code('synthetic-code',CALLBACK,'global')
        return result,captured[0]
    def test_exchange_is_fixed_tls_and_body_only(self):
        result,request=self.exchange({'access_token':'synthetic-access','refresh_token':'synthetic-refresh','expires_in':3600})
        self.assertEqual(request.full_url,'https://auth.voxelshare.com/oauth/token/authCode')
        fields=json.loads(request.data)
        self.assertEqual(fields['auth_code'],'synthetic-code')
        self.assertEqual(fields['client_id'],broker.CLIENT_ID)
        self.assertEqual(fields['redirect_uri'],CALLBACK)
        self.assertEqual(result['access_token'],'synthetic-access')
    def test_camelcase_wrapper_accepted(self):
        result,_=self.exchange({'code':0,'data':{'accessToken':'a','refreshToken':'r','expiresIn':3600}})
        self.assertEqual(result['refresh_token'],'r')
    def test_error_is_not_a_successful_login(self):
        for value in [{'code':1,'data':{}},{'access_token':'a','refresh_token':'','expires_in':3600},{'access_token':'a','refresh_token':'r','expires_in':True},{}]:
            with self.subTest(value=value),self.assertRaises(broker.AuthenticationRejected):self.exchange(value)

class ProcessTests(unittest.TestCase):
    def status(self,error):
        class Session:
            def __init__(self,region):pass
            def run(self,exchange):raise error
        output=io.StringIO()
        with patch.object(broker,'BrowserSession',Session),patch.object(broker.sys,'argv',['broker']),contextlib.redirect_stdout(output),contextlib.redirect_stderr(output):
            status=broker.main()
        self.assertEqual(output.getvalue(),'')
        return status
    def test_cancellation(self):self.assertEqual(self.status(native_browser.LoginCancelled()),5)
    def test_timeout(self):self.assertEqual(self.status(TimeoutError()),3)
    def test_unavailable_callback(self):self.assertEqual(self.status(native_browser.CallbackUnavailable()),7)
    def test_server_rejection(self):self.assertEqual(self.status(broker.AuthenticationRejected()),6)
    def test_unexpected_error_does_not_leak(self):self.assertEqual(self.status(Exception('synthetic-sensitive-data')),4)

class LoopbackIntegrationTests(unittest.TestCase):
    def request(self, path, method='GET', host='localhost:3000', duplicate_host=False):
        connection=http.client.HTTPConnection('127.0.0.1',3000,timeout=3)
        connection.putrequest(method,path,skip_host=True)
        connection.putheader('Host',host)
        if duplicate_host:connection.putheader('Host',host)
        connection.endheaders()
        response=connection.getresponse()
        status,headers,body=response.status,dict(response.getheaders()),response.read().decode('utf-8')
        connection.close()
        return status,headers,body

    def test_real_listener_rejects_invalid_requests_and_consumes_once(self):
        session=native_browser.BrowserSession(timeout=10)
        state=session.state
        completed=[];calls=[];errors=[]
        def exchange(*args):calls.append(args);return {'synthetic_result':True}
        def run():
            try:completed.append(session.run(exchange))
            except Exception as e:errors.append(e)
        with patch.object(native_browser.webbrowser,'open',return_value=True) as launch:
            worker=threading.Thread(target=run,daemon=True);worker.start()
            for _ in range(100):
                if launch.called:break
                if errors:raise errors[0]
                time.sleep(.01)
            self.assertTrue(launch.called)
            launch.assert_called_once_with(session.authorization_url(),new=2)
            valid='/home?'+urllib.parse.urlencode({'signType':'google','state':state,'code':'synthetic-code'})
            for path,method,host,duplicate,expected in [
                ('/favicon.ico','GET','localhost:3000',False,404),
                (valid,'POST','localhost:3000',False,405),
                (valid,'GET','attacker.invalid:3000',False,400),
                (valid,'GET','localhost:3000',True,400),
                (valid.replace(state,'wrong-state'),'GET','localhost:3000',False,400),
                (valid+'&code=duplicate','GET','localhost:3000',False,400),
                (valid.replace('signType=google','signType=facebook'),'GET','localhost:3000',False,400),
            ]:
                self.assertEqual(self.request(path,method,host,duplicate)[0],expected)
                self.assertFalse(session.claimed)
            status,headers,body=self.request(valid)
            self.assertEqual(status,200)
            self.assertEqual(headers['Cache-Control'],'no-store')
            self.assertEqual(headers['Referrer-Policy'],'no-referrer')
            self.assertIn("default-src 'none'",headers['Content-Security-Policy'])
            self.assertNotIn('synthetic-code',body)
            self.assertNotIn(state,body)
            self.assertIn('history.replaceState',body)
            worker.join(3)
        self.assertFalse(worker.is_alive())
        self.assertEqual(errors,[])
        self.assertEqual(completed,[{'synthetic_result':True}])
        self.assertEqual(calls,[('synthetic-code',CALLBACK,'global')])
        self.assertEqual(session.servers,[])
        self.assertIsNone(session.state)
        self.assertIsNone(session.code)
        with self.assertRaises(OSError):self.request(valid)

    def test_busy_port_never_opens_browser(self):
        occupied=socket.socket()
        occupied.bind(('127.0.0.1',3000));occupied.listen()
        session=native_browser.BrowserSession()
        try:
            with patch.object(native_browser.webbrowser,'open') as launch:
                with self.assertRaises(native_browser.CallbackUnavailable):session.run(None)
                launch.assert_not_called()
            self.assertEqual(session.servers,[])
        finally:occupied.close()

    def test_timeout_closes_listener_without_exchange(self):
        session=native_browser.BrowserSession(timeout=.1)
        exchange_calls=[]
        with patch.object(native_browser.webbrowser,'open',return_value=True):
            with self.assertRaises(TimeoutError):session.run(lambda *args:exchange_calls.append(args))
        self.assertEqual(exchange_calls,[])
        self.assertEqual(session.servers,[])
        self.assertIsNone(session.state)

if __name__=='__main__':unittest.main()
