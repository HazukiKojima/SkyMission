# SkyMission 開発・操作ガイド

[作品紹介へ戻る](../README.md)

## ビルド環境

| 項目 | 必要な構成 |
| --- | --- |
| OS | Windows 10 / Windows 11、x64 |
| IDE / C++ | Visual Studio 2026（18.x）、デスクトップC++ワークロード、MSVC v145 |
| Windows SDK | Windows SDK 10.0.26100.0 以降（DirectX 12・FXC・GameInputヘッダーを含む） |
| .NET | .NET 10 SDKとNative AOTに必要なC++ビルドツール |
| GPU | DirectX 12対応GPU。利用できない場合はWARPへフォールバック |

Debug / Releaseのツールセットはv145に統一しています。動作確認の対象はx64です。
`global.json` は.NET 10 SDKを選び、インストール済みの10.0系の最新feature bandへロールフォワードします。
.NET 8 / 9 SDKだけの環境ではビルドできません。`dotnet --version` で10.0系が選ばれることを確認してください。

## 初回ビルドと起動

1. リポジトリを取得し、Visual Studio 2026で `SkyMission.slnx` を開きます。
2. ソリューションのNuGetパッケージを復元します。C++側は `packages.config`、C#側は `External/REDoxWrapper/REDoxWrapper.csproj` の依存関係を使います。
3. スタートアッププロジェクトを `SkyMission`、プラットフォームを `x64` に設定します。
4. 最初に `Debug` をビルドし、Visual Studioから起動します。`Release` も同じv145構成でビルドできます。

ビルド時にはJSONバインディングを生成し、REDoxWrapperをNative AOTのDLLとして発行して実行ファイルの横へコピーします。
シェーダーもCSOへコンパイルします。生成ファイルを手作業で編集する必要はありません。

パッケージ復元をコマンドで行う場合は、NuGet CLIと.NET SDKを用意した上で、リポジトリ直下から実行します。

```powershell
nuget restore packages.config -PackagesDirectory packages
dotnet restore External/REDoxWrapper/REDoxWrapper.csproj
```

Visual Studio 2026のDeveloper PowerShellでは、以下でもビルドできます。

```powershell
MSBuild SkyMission.slnx /m /p:Configuration=Debug /p:Platform=x64
MSBuild SkyMission.slnx /m /p:Configuration=Release /p:Platform=x64
```

開発ビルドは従来どおり `x64/Debug` または `x64/Release` から起動できます。
初回はテクスチャ処理に時間がかかる場合があります。

## 配布用ZIPの作成

PowerShellで `./Tools/Distribution/Package.ps1` を実行します。
Release/x64をビルドし、`artifacts/distribution/日時/` に配布用フォルダーと `SkyMission.zip` を生成します。
ビルド済みのReleaseを使う場合は `-SkipBuild` を指定できます。

```text
SkyMission/
├─ SkyMission.exe          起動専用EXE（これをダブルクリック）
├─ Runtime/
│  ├─ SkyMission.exe       ゲーム本体
│  ├─ DLL類                REDox・Assimp・VCランタイム
│  └─ Shaders/             コンパイル済みシェーダー
├─ Assets/
└─ はじめに.txt
```

ZIPをすべて展開し、一番上の `SkyMission.exe` を起動してください。
起動専用EXEはVCランタイムに依存せず、Runtime内のゲーム本体を起動します。
ゲーム本体のDLL依存関係は同じRuntimeフォルダー内で解決します。
作業ディレクトリに関係なくAssetsとシェーダーを読み込みます。
配布物にはOBJ・PDB・検証ログ・ソースコードを含めません。

## 操作

開始時と再挑戦時には約5秒の敵編隊紹介を再生します。Enterでスキップできます。
演出の時間・編隊配置・カメラ・紹介文・会話字幕・音声パス・音量は `Assets/Cinematics/opening.json` で調整できます。
会話の字幕と音声は同じ項目で管理し、`briefing: []` で会話を省略できます。変更は再起動で反映します。
開始・勝利／敗北で共通のムービー再生処理を使います。空母攻撃への移行時は通常視点のまま母艦を3秒間自動注視し、画角を15％狭めて少し寄ります。飛行・戦闘は継続し、注視後は通常視点と画角へ滑らかに戻ります。
`Assets/Cinematics/cinematics.json` から各演出を参照し、`shots` 配列でカット順・モデル・カメラを指定できます。
途中と終了の演出中は戦闘を停止し、Enterでスキップできます。
実装と検証方法は [ムービー再生の設計](Design/Cinematic.md) を参照してください。

| 入力 | 操作 |
| --- | --- |
| F3（Debugビルドのみ） | コリジョン表示切替。自機：緑、敵機：赤、母艦のミサイル命中領域：黄。遮蔽物越しにも表示 |
| F6 | 撮影用表示を切替：通常 → 空・雲・海・艦船のみ → 空・雲・海のみ。撮影用表示ではHUD・字幕・機体・弾・戦闘エフェクトを非表示。操縦・見回しとゲーム進行は継続 |
| Shift + Enter | 全画面（ボーダーレス）とウィンドウ表示を切り替え |
| W / S | ピッチ操作 |
| A / D | 旋回 |
| Q / E | ヨー操作 |
| Ctrl / Space | 加速 / 減速 |
| Ctrl + Space | ハイG旋回入力 |
| 左クリック長押し | 左右2門の20mm機関砲を連射 |
| Tab | 選択する敵を切り替え |
| 右クリック | 空母攻撃段階で対艦ミサイルを発射（2発装填、4秒で再装填） |
| Z長押し | 選択中の敵機を自動注視。離すと標準視点へ復帰。ホイール押し込み中は手動見回しを優先 |
| マウスホイールを押しながらマウス移動 | 機体の周囲を見回す。ホイールを離すと標準視点へ滑らかに復帰 |

WindowsではGameInputを使って、Xboxコントローラー、DualShock 4、DualSenseでも操作できます。
WindowsのGameInputランタイムが利用できる環境が必要です。

| Xbox | PS4 / PS5 | 操作 |
| --- | --- | --- |
| A | × | 開始、クリア後にタイトルへ戻る |
| Menu | OPTIONS / オプションボタン | 演出スキップ |
| 左スティック | 左スティック | 上下でピッチ、左右で旋回 |
| LB / RB | L1 / R1 | 左 / 右ヨー |
| RT / LT | R2 / L2 | 加速 / 減速。両方を深く押すとハイG旋回入力 |
| A 長押し | × 長押し | 機関砲を連射 |
| B | ○ | 対艦ミサイルを発射 |
| Y を短く押して離す | △ を短く押して離す | 選択する敵を切り替え |
| Y を0.3秒以上長押し | △ を0.3秒以上長押し | 選択中の敵機を自動注視 |
| 右スティック | 右スティック | 機体の周囲を見回す |

振動対応コントローラーでは、機関砲の射撃、対艦ミサイルの発射、敵機と母艦の撃破、自機の被弾、加減速、ハイG旋回で本体が振動します。加速は入力に応じた低い振動、減速は速度と入力に応じた細かい振動になります。

振動を調整するには [Assets/Combat/rumble.json](../Assets/Combat/rumble.json) を編集してゲームを再起動します。
`lowFrequency` と `highFrequency` はそれぞれ 0〜1 の強さで、両方を 0 にするとその効果の振動を無効にできます。
`durationSeconds` は振動時間、`gun.repeat.intervalSeconds` は連射中の振動の間隔です。連射の振動時間は間隔以下にしてください。
`responsePerSecond` を大きくすると加減速時の振動の立ち上がりと収まりが速くなります。省略した項目には既定値が使われます。

HUDのHULLは自機の耐久力です。画面上部には現在目標と残存機数を表示します。
空母攻撃へ移行すると母艦を自動選択し、方向矢印と六角形の目標枠を表示します。
敵機と同じ条件で母艦を機首方向に2秒間捉えるとロックが成立し、右クリックで対艦ミサイルを発射できます。
母艦は遠距離や後方でも選択を維持しますが、未ロック時は発射せず弾数も消費しません。
ミサイルは母艦へ誘導され、1発命中すると母艦が爆発します。2発を撃ち切ると再装填し、外しても再挑戦できます。機関砲では母艦を撃破できません。
誘導・移動・命中判定は最大1/240秒ごとに更新します。予測マーカーには次に発射する左右の位置と機体速度を反映しますが、発射後の誘導による旋回は含みません。
空母攻撃後はCOMBAT AREAマーカーを目印に艦隊の外側へ向かい、残存機を撃墜します。
編隊長機は通常機の1.5倍の耐久力と短い射撃間隔を持ちます。

機首を敵へ向けて捕捉を維持するとロックが成立します。ロックは機関砲の未来位置照準と小さな射撃補正に使用します。
左クリック長押しで機関砲を連射し、Tabで敵を切り替えます。

### 入力と描画の確認

フォーカスを失うと入力を解除します。最小化中はシミュレーションと描画を停止し、復帰時に経過時間をリセットします。

`Assets/Sound/Music/BattleBGM.mp3` はゲーム開始後にステレオで繰り返し再生します。フォーカス喪失・最小化中は一時停止し、復帰すると続きから再生します。

タイトルバーにはFPSとGPU描画時間を表示します。Seaは海、Modelsはモデル、Postは海霧とフィルム効果です。
GPU時間は数フレーム遅れて取得し、計測のためのGPU待機は追加しません。
海の距離別メッシュ、視野外カリング、艦船の3段階LOD、静止メッシュの結合、後処理の統合を実装しています。
仕組みと検証結果は[描画の最適化](Design/RenderingPerformance.md)を参照してください。

## コード規約

C++とHLSLの書式はそれぞれの`.clang-format`で管理しています。
命名、日本語コメント、音声の責務とHLSL警告の調査結果は、[可読性とシェーダー警告の設計書](Design/ShaderWarningsAndCodeStyle.md)を参照してください。

