# SkyMission

C++20 / DirectX 12で、航空機の操縦・空戦・海洋環境を実装したWindows向けの技術デモです。
プレイヤー機は紫電改（N1K2-J）、敵機はF4U-4 Corsairを使用します。

## 実装している機能

- クォータニオン姿勢、推力・揚力・抗力を使った航空機の移動、敵機の巡回AI
- 左クリックによる4門の20mm機関砲、Fキーによる左右交互発射の軽追尾ロケット
- 敵の選択・ロックオン、未来位置照準、移動OBBとの区間衝突判定、ダメージと撃破
- HDRテクスチャを使ったスカイスフィア、波・法線マップ・霧を使った海洋描画
- レトロフィルムの色調補正、照準・敵枠・武器状態のHUD、着弾と撃破の演出
- Raw Inputによる入力スナップショット、JSON設定、C++ / C# Native AOT間の連携

雲はHDR空テクスチャに含まれる表現です。ボリュメトリッククラウドは実装していません。
現在は敵3機との戦闘デモで、敵の再出現・反撃・ミッション選択はありません。

## ビルド環境

| 項目 | 必要な構成 |
| --- | --- |
| OS | Windows 10 / Windows 11、x64 |
| IDE / C++ | Visual Studio 2026（18.x）、デスクトップC++ワークロード、MSVC v145 |
| Windows SDK | Windows 10 / 11 SDK（DirectX 12・FXCを含む） |
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

実行時は `Assets` と出力先の `x64/Debug` または `x64/Release` の相対配置を維持してください。
EXEだけを別の場所へ移動すると、アセットと外部DLLを読み込めません。初回はテクスチャ処理に時間がかかる場合があります。

## 操作

| 入力 | 操作 |
| --- | --- |
| W / S | ピッチ操作 |
| A / D | 旋回 |
| Q / E | ヨー操作 |
| Ctrl / Space | 加速 / 減速 |
| Ctrl + Space | ハイG旋回入力 |
| 左クリック長押し | 4門の20mm機関砲を連射 |
| Tab | 選択する敵を切り替え |
| F押下 | L1 → R1の順でロケットを1発発射 |

機首を敵へ向けて0.5秒捕捉するとロックが成立します。機関砲は未来位置照準へ向けて狙い、条件内では小さな射撃補正が働きます。
ロケットは発射時のロック対象を保持して緩く追尾し、ロックなし・対象消失時は直進します。
L1 / R1には独立した3秒のクールタイムがあり、HUD左下で次の銃口とREADY / 残り秒数を確認できます。

フォーカスを失うと入力を解除します。最小化中はシミュレーションと描画を停止し、復帰時に経過時間をリセットします。

## 構成と主な責務

| コンポーネント | 担当 |
| --- | --- |
| Application | 初期化、ウィンドウのメッセージ処理、更新と描画の呼び出し |
| FlightScene / FlightSceneLoader | 戦闘シーンの更新順、設定・アセット定義の読み込み |
| AircraftFlightModel / AircraftAIController | 飛行モデルと敵機の操縦入力 |
| TargetingSystem / GunAimSolver | 対象選択・ロック状態と迎撃位置の計算 |
| AircraftGun / ProjectileSystem | 機関砲の発射タイミング、弾の移動と命中収集 |
| RocketLauncher / RocketSystem | 銃口別クールタイム、追尾、ロケットの移動と命中収集 |
| CombatEffectSystem / CombatEffectPresenter | 演出の生成・寿命管理と描画データの作成 |
| CombatHudPresenter / HudRenderer | 戦闘情報の表示とGPUへの描画 |
| Renderer / RenderDevice / CommandContext | 描画パス、D3D12リソース、コマンド記録とGPU同期 |
| JsonAssetReader / JsonParametersLoader | JSONからのパス・値の取得と型付き設定の受け渡し |

`Engine` には描画・リソース・入力などを、`Game` には航空機・武器・戦闘・表示を配置しています。

## 技術上の工夫

- 永続参照にはObjectIdを使い、オブジェクト追加・削除で無効になるポインターを保持しません。
- 命中を収集してからフレーム内時刻順にダメージを反映し、走査途中の敵削除や撃破通知の重複を防ぎます。
- モデル・海・空の静的メッシュはDEFAULTヒープへ転送し、GPU完了後にUPLOAD用リソースを解放します。
- フレーム別のコマンドアロケータ・定数バッファとフェンスで、GPU使用中の領域の再利用を制御します。
- JSONとモデルの読み込みを発射処理から分離し、弾の上限に合わせて描画容量を先に確保します。
- C++ヘッダーからC#の設定バインディングと配置検証用static_assertを生成し、Native AOTとのデータ配置を検証します。
- 海と空の設定は読み込み時に範囲を検証し、不正な項目名とファイルパスをエラーへ含めます。

## 設定ファイル

| 対象 | 設定場所 |
| --- | --- |
| プレイヤー機 | `Assets/Aircraft/N1K2JShidenKai/` |
| 敵機・AI | `Assets/Aircraft/F4U4Corsair/` |
| 機関砲 / ロケット | 機体フォルダーの `weapon.json` / `rocketWeapon.json` |
| 弾薬モデル | `Assets/Ammunition/20mmAmmo/` / `Assets/Ammunition/Rocket/` |
| 敵配置・耐久値・ロック条件 | `Assets/Combat/targeting.json` |
| HUD | `Assets/UI/combatHud.json` |
| 海・空・フィルム効果 | `Assets/Environment/` |

モデルの表示サイズは各 `modelImport.json` の `normalizedExtent` で指定します。変更後はアプリを再起動してください。
演出の寿命・本数・描画上限は `Game/Effects/CombatEffectSettings.h` にまとめています。
機関砲には4門のマズルフラッシュ、2発に1発の曳光弾、薄い発射煙を表示します。
調整値と描画の制約は[機関砲の発射エフェクト設計](Docs/Design/GunFireEffects.md)を参照してください。
詳細は [機関砲の設計](Docs/Design/PlayerAircraftGun.md) と [ロックオン・HUDの設計](Docs/Design/TargetLockCombatHud.md) を参照してください。

## テスト

Debug x64を一度ビルドしてNative AOT DLLを生成した後、リポジトリ直下で実行します。

```powershell
./Tests/RunRegressionTests.ps1
```

テストは一時ディレクトリでC++とHLSLをコンパイルします。主な検証対象は、飛行モデル、Raw Input、フォーカス喪失、ウィンドウ操作、武器の発射間隔・クールタイム境界、追尾・命中・撃破、設定の不正値、テクスチャ、GPU読み戻し、HUDの投影・リサイズ、描画パスです。
実アセットの読み込み検証と、固定条件での武器挙動検証を分けています。
HLSLは最適化なし・ありの両方で警告をエラーとして検出し、音声の再初期化とCOM所有権も検証します。

## コード規約

C++とHLSLの書式はそれぞれの`.clang-format`で管理しています。
命名、日本語コメント、音声の責務とHLSL警告の調査結果は、[可読性とシェーダー警告の設計書](Docs/Design/ShaderWarningsAndCodeStyle.md)を参照してください。

## 使用ライブラリと現在の制約

Assimpによるモデル読み込み、DirectXTexによる画像処理、RE:Dox / REDoxWrapperによるJSON処理を利用しています。
モデル・画像は `Assets` に配置しています。

- 飛行モデルはゲーム向けの近似です。
- 命中判定は機体全体を囲むOBB近似で、メッシュ単位ではありません。
- 爆発・煙・着弾はHUD描画経路による投影表示で、3D物体による遮蔽は行いません。
- ロケットは直撃ダメージのみで、爆風ダメージと自機判定はありません。
- WARPへのフォールバックは機能確認用で、実機GPUと同じ性能は保証しません。
