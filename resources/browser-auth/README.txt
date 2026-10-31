FlashStudio browser authentication / ブラウザ認証

English
Google sign-in uses the normal system default browser and returns to the app.
No browser extension, browser automation, private profile or cookie access is used.
The loopback listener validates the exact Host, path, provider, random state,
duplicate parameters, code size and a five-minute deadline before one-time exchange.
The registered return address is http://localhost:3000/home?signType=google.
The official Flashforge HTTPS API exchanges the code. This API does not accept
a PKCE verifier. If port 3000 is occupied, the attempt fails before browser launch.
Tokens are returned only through the parent's anonymous pipe. Do not run the
helper directly in a terminal for a real login. Existing app token storage is unchanged.
Google/Windows is tested. Apple/Facebook and macOS/Linux are not verified.

日本語
Google認証を普段の既定ブラウザで行い、アプリへ認証結果を返します。
拡張機能、ブラウザの自動操作、専用プロフィール、Cookieの読み取りは使いません。
PC内の受信先でHost、パス、認証元、乱数state、重複引数、コード長と5分期限を検査し、
認証コードを1回だけ交換します。戻り先は http://localhost:3000/home?signType=google です。
Flashforge公式のHTTPS APIで交換します。このAPIにPKCE verifierの引数はありません。
ポート3000が使用中の場合はブラウザを開く前に失敗として表示します。
トークンは親アプリの匿名パイプ専用です。実ログインのためにヘルパーを端末から直接
起動しないでください。本体の既存トークン保存方法は変更していません。
Google/Windowsを検証済みです。Apple/FacebookとmacOS/Linuxは未検証です。

Tests / テスト (from this directory / このフォルダーで実行)
python -m unittest -v test_broker

Build / ビルド
python -m PyInstaller --onefile --console --name flashstudio-browser-auth --distpath . broker.py

License / ライセンス: root LICENSE.txt (AGPL-3.0), CREDITS.txt, LICENSE_PYTHON.txt.
