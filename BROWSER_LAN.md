# Browser login and LAN printer addition / ブラウザ認証とLANプリンター追加

## English

- Google sign-in opens in the system default browser and returns to the app. No extension or private browsing is required.
- Account verification has a timeout, cancellation and retry controls.
- The + button opens a printer discovery dialog with refresh and access-code connection. LAN addition does not require a cloud account.
- Fixes initial dialog painting and improves Japanese UI text.
- Removes Google-page script injection, suppresses authentication payload logging and enables TLS certificate verification in the HTTP client.

Tested: Windows x64 Release build, 22 authentication tests, Japanese catalog checks, real Google sign-in and Creator 5 Pro LAN addition. macOS/Linux and Apple/Facebook sign-in are not verified.

Build on Windows from an x64 C++ developer shell with Visual Studio 2022, CMake, Ninja, Perl, Python and PyInstaller:

```powershell
.\build_browser_lan.ps1 -OriginalApplicationDirectory 'C:\path\to\official-1.7.15'
```

Use the matching FlashNetwork 3.4.3 DLL and FLASHNETWORK9.DAT from official 1.7.15. The script validates the SDK and builds the browser helper. See [helper details](resources/browser-auth/README.txt).

## 日本語

- Google認証をPCの既定ブラウザで行い、認証結果をアプリへ戻します。拡張機能やシークレット起動は不要です。
- アカウント確認に待ち時間の制限、キャンセル、再試行を用意しました。
- ＋ボタンからプリンターの検索・再検索・アクセスコード接続へ進めます。LAN追加にクラウドへのログインは不要です。
- ダイアログを開いた直後の表示欠けと、日本語UIの表記を修正しました。
- Googleページへのスクリプト注入を削除し、認証情報のログ出力を抑制、HTTP通信の証明書検証を有効化しました。

検証済み：Windows x64 Releaseのビルド、認証22テスト、日本語カタログ検査、実アカウントのGoogle認証、Creator 5 ProのLAN追加。macOS/LinuxとApple/Facebookの認証は未検証です。

上記のビルドコマンドを使用してください。公式1.7.15に含まれるFlashNetwork 3.4.3のDLLとFLASHNETWORK9.DATが必要です。スクリプト内でSDKを検査し、認証用プログラムもビルドします。
