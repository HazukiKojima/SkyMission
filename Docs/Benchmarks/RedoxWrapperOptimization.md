# RE:DOX本体を変更しない設定読み込みの高速化

**比較上の注意：この記録ではREDox側だけをゲーム向けに最適化している。**
ライブラリ間の順位には、後から実施した[3者へ同じ最適化を適用した比較](JsonParserMatchedOptimization.md)を参照。
以下の変更前後の計測は、ゲーム側ラッパーの改善確認として残している。

2026-11-09。ゲーム側のラッパーとC++の受け取り処理だけを変更した。
CAPCOM.REDoxは1.0.3のまま。NuGetパッケージのソース、DLL、バージョン、生成済み構造体の配置は変更していない。

## 実装

- JSONはRE:DOXの既存UTF-8 APIへ直接渡す。文書作成・従来の一回読み込みAPIの文字列変換を省く。
- 数値設定の項目名・要素数・出力位置を`NumericParameterLayout`に集約する。
  プロパティを一度列挙して、生成済みC++構造体と同じ順番のfloat列へ配置する。
- `ParsedJsonDocument`が文書と型付き取得結果を所有する。同じ文書・配置の再取得はfloat列のコピーだけで済ませる。
  文書の解放時にキャッシュを破棄し、ラッパーの既存ロックで同時アクセスを保護する。
- 個別の数値・文字列取得もUTF-8の項目名で検索し、項目名の文字列変換を省く。
- C++の`JsonParametersLoader`は一時`vector<float>`を固定長`array`へ変更し、小さい設定読み込みのヒープ確保を省く。

既存の公開エントリーポイント、出力配置、エラーコードを維持する。
数値の有限性・必要項目・配列長を検証し、失敗時は出力やキャッシュへ不完全な値を書き込まない。
RE:DOX 1.0.3の検索と同じく、重複したキーは最初の値を使用する。
新しい補助ファイルもDLL再発行の入力に加え、変更時に古いDLLを使い続けないようにした。

## 再計測

前回と同じC++ /O2、Native AOT、Ryzen 5 5600X、3プロセス×7標本。
旧方式、UTF-8直接入力、System.Text.Json、nlohmann/jsonと、ゲームの最適化済みソースを同じ実行で比較した。
単位はµs/操作、値は21標本の中央値。

| 処理 | 旧方式の比較経路 | ゲーム側最適化後 | STJ UTF-8 | nlohmann/json |
| --- | ---: | ---: | ---: | ---: |
| AI設定の解析・破棄 | 0.904 | 0.757 | 1.658 | 9.019 |
| AI設定の解析＋30項目取得 | 8.344 | **2.848** | 4.573 | 10.161 |
| 文書から30項目を一括再取得 | 7.343 | **0.051** | 2.753 | 0.988 |
| 文書から30項目を個別に取得 | 8.405 | 6.474 | 3.839 | **0.987** |
| 実設定24ファイルの解析・破棄 | 12.982 | **11.790** | 23.972 | 119.336 |

AI設定の解析から一括取得までは旧方式の約2.93倍の速度で、今回のSTJ/nlohmann経路も上回った。
再取得0.051µsは**検証・取得済みfloat列のキャッシュを使う時間**。
RE:DOX本体の値検索がこの速度になったという意味ではない。
個別取得は約23%短縮したが、STJやnlohmannより速くなったわけではない。

一括キャッシュを使わない文書でも、小さな管理オブジェクトとキャッシュ用の空辞書を保持する。
初回一括読み込みには、検証済みfloat列と辞書の追加確保がある。
その初回の確保も「解析＋30項目取得」の計測に含まれる。

最適化後の比較用DLLはゲーム本体の`REDoxApi.cs`と補助クラスを直接コンパイルしたもの。
GC制御の計測用エクスポートだけを比較プロジェクトに追加し、本番DLLの公開APIには加えない。
旧方式は[前回の比較用ブリッジ](../../Tools/JsonParserBenchmark/README.md)のまま保持した。
ディスクI/O、DLLロード、ゲーム全体の起動時間やFPSは測っていない。

[全標本と集計](JsonParserResults/2026-11-09-wrapper-optimized/native-summary.csv)、
[全ケースの表](JsonParserResults/2026-11-09-wrapper-optimized/table.md)、
[入力のハッシュ](JsonParserResults/2026-11-09-wrapper-optimized/input-hashes.csv)。
元の比較結果は別ディレクトリに保存し、上書きしていない。

## 検証と再現

```powershell
# 実装ソースを共有する互換性・キャッシュ・寿命・並行アクセスのテスト
dotnet run --project Tools/RedoxWrapperTests/RedoxWrapperTests.csproj -c Release -- .

# 最適化したゲーム側実装も含めて比較。出力先を分けて前回結果を保持する
./Tools/JsonParserBenchmark/RunBenchmark.ps1 `
  -OutputDirectory x64/JsonParserOptimizationBenchmark -IncludeGameOptimized

./Tools/JsonParserBenchmark/Summarize.ps1 `
  -InputDirectory x64/JsonParserOptimizationBenchmark `
  -OutputDirectory x64/JsonParserOptimizationBenchmark/summary
```

新しいNative AOT DLLを`-RedoxDll`で指定し、C++から以下も確認した。

```powershell
./Tests/RunRegressionTests.ps1 -BuildDirectory x64/RedoxOptimizationTests `
  -RedoxDll x64/RedoxOptimization/REDoxWrapper.dll -JsonReview
```

- JSON・AI配置・環境設定・艦船・開始ムービー設定の既存テストを通過。
- 同じDLLで敵AI・武器・任務遷移・HUDの`--mission-review`も通過。
- 項目順、未知の項目、大小のスキーマの重複キー、欠落、不正型、非有限値、配列長、エラー時の出力不変を検証。
- 入力UTF-8バッファを破棄しても文書が有効であること、日本語キーと文字列、呼び出し元による出力変更からの独立性を検証。
- ハンドル解放後の拒否、複数スレッドによる作成・取得・解放を検証。

起動時の固定設定向けの改善であり、実行中にJSON文書を編集する仕組みは追加していない。
設定変更を読む場合は従来どおり文書／ローダーのキャッシュを作り直す。
