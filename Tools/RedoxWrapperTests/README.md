# ゲーム側RE:DOXラッパーのテスト

```powershell
dotnet run --project Tools/RedoxWrapperTests/RedoxWrapperTests.csproj -c Release -- .
```

ゲーム側の実装ソースを直接コンパイルして、公開ネイティブABIを関数ポインターから呼び出す。
RE:DOX本体やバージョンは変更しない。
項目順・重複キー・欠落・不正値・配列長・エラー時の出力不変・キャッシュの独立性・UTF-8所有権・
日本語・ハンドル寿命・並行アクセスを確認する。
Native AOT実DLLを使うC++回帰テストと性能比較は
[高速化の検証記録](../../Docs/Benchmarks/RedoxWrapperOptimization.md)を参照。
