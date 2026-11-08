# JSON parser benchmark

C++ゲームからの利用を主評価とし、REDox 1.0.3、.NET 10.0.14のSystem.Text.Json、
nlohmann/json 3.12.0を比較する。本体のパーサーや依存設定は変更しない。

```powershell
./Tools/JsonParserBenchmark/RunBenchmark.ps1 -ManagedSupplement
```

Visual Studio C++ x64ツール、.NET 10 SDK、Native AOTツールチェーンが必要。
比較用nlohmannヘッダーは公式の固定タグから出力ディレクトリへ取得し、SHA-256を検証する。
標準出力の時間は各プロセスの中央値。CSVには全標本を保存する。

## 主評価：C++ /O2からの呼び出し

| 経路 | 内容 |
| --- | --- |
| REDox-current | UTF-8→文字列→REDox DOM。現行ゲームと同様の変換・ハンドル管理・ロックを再現 |
| REDox-utf8 | 同じNative AOTブリッジからREDoxのUTF-8 APIを使用 |
| STJ-current | REDox-currentと同一ソースのブリッジをSystem.Text.Jsonでビルド |
| STJ-utf8 | 同じブリッジで入力を所有するbyte[]へコピーし、UTF-8 APIで解析 |
| nlohmann-json | C++からUTF-8を直接解析。標準の`nlohmann::json`（既定のmap/可変DOM） |

元のC++入力が解放されても解析済み文書を使える所有権をそろえる。
ライブラリ読込・JIT/初期化はウォームアップへ分離する。
実際のゲームのDLLそのものではなく、解析経路とハンドル操作を再現した比較用DLLを使用する。
フィールド取得は生成済みの30項目名とfloat値のチェックサムを一致させてから測定する。

- 実際のAI設定、Assets内の全JSON、数値配列1万/10万要素、入れ子・日本語を含む機体1000件を使用。
- DOM構築＋破棄、解析＋30項目取得、解析済み文書から一括/個別の30項目取得を分ける。
- 一括取得は1回のブリッジ呼出し、個別取得は30回。nlohmannはどちらもC++内で取得する。
- 3プロセス×7標本、各標本約100ms。毎回測定順を巡回させる。計測を並行させない。
- ディスクI/O、C++側のDLL関数アドレス解決、ゲーム側キャッシュ管理、DTOデシリアライズは含めない。
- .NET側は各標本前にGCを実行する。GC呼出し自体は測定外、標本中に起きたGCは時間へ含む。
- メモリ使用量は主評価の比較対象外。管理ヒープだけの値でC++やネイティブメモリの優劣を判断しない。

## 補足：純.NET JIT

`-ManagedSupplement`指定時は、別プロセスで.NET内部の文字列/UTF-8 DOM解析と30項目取得を測る。
TieredCompilationとReadyToRunを無効化し、ウォームアップ後に9標本を取得する。
ここでの割当量は現在スレッドの管理割当のみで、ネイティブ領域やプールの保有容量は含まない。
主評価のC++→Native AOT経路とは別の結果として扱う。

固定バージョンの参照元：[nlohmann/json 3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)、
[REDox 1.0.3](https://github.com/CAPCOM-TD-OSS/redox/tree/v1.0.3)。
