# SkyMission

C++20 / DirectX 12で、航空機の操縦・空戦・艦隊攻撃を実装したWindows向けの単独迎撃ゲームです。
プレイヤー機は紫電改（N1K2-J）、敵機はF4U-4 Corsairを使用します。

## 実装している機能

- クォータニオン姿勢、推力・揚力・抗力を使った航空機の移動、敵機の遠距離攻撃・接近回避AI
- 左クリックによる4門の20mm機関砲、右クリックによる対艦ミサイル（2発装填、4秒で再装填）
- 敵の選択・ロックオン、未来位置照準、移動OBBとの区間衝突判定、ダメージと撃破
- HDRテクスチャを使ったスカイスフィア、波・法線マップ・霧を使った海洋描画
- レトロフィルムの色調補正、照準・敵枠・武器状態のHUD、着弾と撃破の演出
- Raw Inputによる入力スナップショット、JSON設定、C++ / C# Native AOT間の連携

少量の厚い積雲と、高度14〜16kmの薄い筋雲を組み合わせて描画します。空の色と既存の色調設定を維持します。
厚い雲は縦横1/3解像度で計算して深度を考慮して拡大合成し、薄い筋雲はフル解像度で描画します。
1945年の太平洋を舞台に、紫電改で基地へ迫る艦隊を迎撃する一つのミッションです。
哨戒機4機の撃墜 → 空母へのミサイル攻撃 → 残存機4機と編隊長機1機の撃墜、の順に進行します。
空母はミサイル2発の命中で撃破でき、船体は残骸として残ります。護衛艦2隻は対空砲火を行います。
敵機は3〜6kmを中心に、接近・直進離脱・反転・再攻撃を繰り返します。母艦から12kmを超えると8km以内まで戻り、母艦側へ誘うように交戦します。ミッション2では母艦をロックオンして対艦ミサイルを誘導できます。
4機は前後・左右・高度をずらして配置し、各機が独自の離脱地点・飛行距離・再接近経路を持ち、反転のタイミングと場所、旋回角、上昇・降下が異なります。
自機の初速は500m/s、敵は700m/s。敵は4機とも射撃し、通常機の機銃間隔は約0.22秒で、各機が異なるタイミングで短く連射し、間に休止を挟みます。敵弾は発射後に曲がりません。
最後の敵機を撃墜し、自機が生存していれば勝利です。自機撃墜と最後の敵機撃墜が同フレームなら敗北を優先します。
勝利・敗北後はRキーで最初から再挑戦できます。帰還操作・時間制限・ミッション選択はありません。

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

開始時と再挑戦時には約5秒の敵編隊紹介を再生します。Enterでスキップできます。
演出の時間・編隊配置・カメラ・紹介文・会話字幕・音声パス・音量は `Assets/Cinematics/opening.json` で調整できます。
会話の字幕と音声は同じ項目で管理し、`briefing: []` で会話を省略できます。変更は再起動で反映します。
実装と検証方法は [開始ムービーの設計](Docs/Design/OpeningCinematic.md) を参照してください。

| 入力 | 操作 |
| --- | --- |
| Shift + Enter | 全画面（ボーダーレス）とウィンドウ表示を切り替え |
| W / S | ピッチ操作 |
| A / D | 旋回 |
| Q / E | ヨー操作 |
| Ctrl / Space | 加速 / 減速 |
| Ctrl + Space | ハイG旋回入力 |
| 左クリック長押し | 4門の20mm機関砲を連射 |
| Tab | 選択する敵を切り替え |
| R | 勝利・撃墜後に再挑戦 |
| 右クリック | 空母攻撃段階で対艦ミサイルを発射（2発装填、4秒で再装填） |
| Zを押しながらマウス移動 | 機体の周囲を見回す。Zを離すと標準視点へ滑らかに復帰 |

HUDのHULLは自機の耐久力です。画面上部には現在目標と残存機数を表示します。
空母攻撃へ移行すると母艦を自動選択し、方向矢印と六角形の目標枠を表示します。
敵機と同じ条件で母艦を機首方向に2秒間捉えるとロックが成立し、右クリックで対艦ミサイルを発射できます。
母艦は遠距離や後方でも選択を維持しますが、未ロック時は発射せず弾数も消費しません。
ミサイルは母艦へ誘導されます。2発を撃ち切ると再装填し、外しても再挑戦できます。機関砲では母艦を撃破できません。
誘導・移動・命中判定は最大1/240秒ごとに更新します。予測マーカーには次に発射する左右の位置と機体速度を反映しますが、発射後の誘導による旋回は含みません。
空母攻撃後はCOMBAT AREAマーカーを目印に艦隊の外側へ向かい、残存機を撃墜します。
編隊長機は通常機の1.5倍の耐久力と短い射撃間隔を持ちます。

機首を敵へ向けて捕捉を維持するとロックが成立します。ロックは機関砲の未来位置照準と小さな射撃補正に使用します。
左クリック長押しで機関砲を連射し、Tabで敵を切り替えます。

フォーカスを失うと入力を解除します。最小化中はシミュレーションと描画を停止し、復帰時に経過時間をリセットします。

`Assets/Sound/BattleBGM.wav` はゲーム開始後にステレオで繰り返し再生します。フォーカス喪失・最小化中は一時停止し、復帰すると続きから再生します。

タイトルバーにはFPSとGPU描画時間を表示します。Seaは海、Modelsはモデル、Postは海霧とフィルム効果です。
GPU時間は数フレーム遅れて取得し、計測のためのGPU待機は追加しません。
海の距離別メッシュ、視野外カリング、艦船の3段階LOD、静止メッシュの結合、後処理の統合を実装しています。
仕組みと検証結果は[描画の最適化](Docs/Design/RenderingPerformance.md)を参照してください。

## 構成と主な責務

| コンポーネント | 担当 |
| --- | --- |
| Application | 初期化、ウィンドウのメッセージ処理、更新と描画の呼び出し |
| FlightScene / FlightSceneLoader | 戦闘シーンの更新順、設定・アセット定義の読み込み |
| FlightMission / FlightSceneRestart | ミッション進行・増援・艦隊攻撃と、再挑戦状態の保存・復元 |
| CombatSnapshotBuilder | HUD向けの飛行情報と照準計算 |
| AircraftFlightModel / AircraftAIController | 飛行モデルと敵機の操縦入力 |
| TargetingSystem / GunAimSolver | 対象選択・ロック状態と迎撃位置の計算 |
| AircraftGun / ProjectileSystem | 機関砲の発射タイミング、弾の移動と命中収集 |
| CarrierMissileSystem / CarrierMissileFlight | 対艦ミサイルの発射・再装填・誘導・命中収集と予測位置 |
| BombSystem / BombFlight | 独立した投下爆弾の自由落下と命中処理（現在のミッションでは未使用） |
| CombatEffectSystem / CombatEffectPresenter | 演出の生成・寿命管理と描画データの作成 |
| CombatHudPresenter / HudRenderer | 戦闘情報の表示とGPUへの描画 |
| Renderer / RenderDevice / CommandContext | 描画パス、D3D12リソース、コマンド記録とGPU同期 |
| JsonAssetReader / JsonParametersLoader | JSONからのパス・値の取得と型付き設定の受け渡し |

`Engine` には描画・リソース・入力などを、`Game` には航空機・武器・戦闘・表示を配置しています。
本番シーンは `FlightScene::CreateMission` で一括生成します。生成条件・フレーム更新順・再挑戦の扱いは[シーンのライフサイクル](Docs/Design/FlightSceneLifecycle.md)を参照してください。モデルの取り込み・形状処理・GPU管理の分担は[モデル処理の設計](Docs/Design/ModelProcessing.md)に記載しています。

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
| 機関砲 / 爆弾 | 自機フォルダーの `weapon.json` / `Assets/Ammunition/Bomb/ammunition.json` |
| 弾薬モデル | `Assets/Ammunition/20mmAmmo/` / 爆弾定義から参照する既存モデル |
| 耐久値・ロック条件 | `Assets/Combat/targeting.json` |
| ミッション機数・再装填・対空砲・甲板判定 | `Assets/Combat/mission.json` |
| HUD | `Assets/UI/combatHud.json` |
| 海・空・フィルム効果 | `Assets/Environment/` |
| 海上の艦船配置 | `Assets/Ships/ships.json` |

艦船は開始地点の前方に各1隻を静止配置します。先頭が目標空母、続く2隻が護衛艦です。`ships.json` の `shipCount` と
`ship1` ～ `shipN` の各項目で変更できます。`Model` はこのJSONからの相対パス、
`Position` はワールド座標、`RotationDegrees` はX・Y・Z軸の回転角（度）です。
`Size` はモデルの最長辺を正規化した後のワールドサイズ、`WaterlineOffset` は配置Yへの加算値です。
船底を海面下へ沈める場合は `WaterlineOffset` を負にします。変更は次回起動時に反映されます。
`maxTextureDimension` は艦船テクスチャの長辺上限（初期値2048）です。元画像を保持したまま、
読み込み時に縮小してミップ生成・圧縮します。近距離の画質を優先する場合は4096へ変更できます。

FBXの解析結果は一時フォルダー内の `SkyMissionModelCache`、処理済みテクスチャは
`SkyMissionTextureCache` に保存します。初回とアセット変更後は生成処理が走り、次回以降は再利用します。
キャッシュを保存できない環境や読み込みに失敗した場合は、元のアセットから読み込みます。
モデルごとにGPU転送を完了してから転送用メモリを解放し、初期化中のメモリ増加を抑えます。

C++の共通ヘッダーはPCHで共有し、コンパイルは最大2並列にしています。Native AOT DLLは
入力ファイルの変更時だけ発行し、変更のないビルドでは発行とコピーを省略します。

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

起動時はモデルとGPUリソースを別スレッドで準備し、読み込み中の表示とウィンドウ操作を維持します。
読み込み中に閉じた場合は、GPU初期化が終わってから安全に終了します。
画像の処理済みDDSはHDRを含めて一時フォルダーの`SkyMissionTextureCache`へ保存し、
次回は展開とミップ生成を省略します。元画像の更新や読み込み設定の変更でキャッシュを切り替えます。
FBXは変換済みの頂点・LOD・材質参照・ノード位置を`SkyMissionModelCache`へ保存し、
次回はAssimpによる再展開と形状・LODの再生成を省略します。初回とモデル・形状設定の変更時は再生成します。
材質の画像は毎回解決するため、テクスチャの差し替えは通常どおり反映されます。
キャッシュが破損した場合や書き込めない場合も、元モデルから読み込みます。
Debugでもモデルの形状加工・LOD生成・キャッシュ処理は速度優先でコンパイルします（シンボルとDebug CRTは維持）。
これらのファイルではステップ実行やローカル変数の参照が制限される場合があります。
デバッグ出力の`Model '...' ready in ... ms`と`Startup completed in ... ms`で所要時間を確認できます。
さらに環境変数`SKYMISSION_STARTUP_LOG`にログファイルの絶対パスを指定すると、
工程別の時間を追記します（モデル取り込み、キャッシュ読み込み、材質・GPU転送、描画初期化、音声など）。

積雲はピクセルシェーダーで雲の内部を積分して描画します。
高度1,800〜4,600mに、雲底と頂上の高さが異なる積雲を少数配置します。開始地点周囲は上空を開け、遠景にも空の隙間を残した晴天です。風による移動と雲底の陰影も表現します。
シーンの深度までで積分を止めるため、手前の機体・艦船を遠方の雲が覆うことはありません。
`Assets/Environment/Clouds.json`で高度・雲量・濃さ・大きさ・風を調整できます。
`bottom` / `top` / `scale` / `maxDistance`はメートル、`wind`は[X,Z]方向のm/sです。
`maxDistance`は雲の色を大気へなじませ始める距離です。遠景にも空の隙間を残します。
`coverage`は雲を発生させるセルの割合（0〜1、初期値0.24）、`density`は光の減衰密度です。`steps`は品質設定（16〜64、初期値48）で、雲1つの交差区間を16〜32点で積分します。
`enabled: 0`または設定ファイルの省略で無効になります。設定変更は再起動で反映します。
描画画像と深度の既存バッファを使い、大きな画像・3Dテクスチャ・雲モデルの読み込みは追加しません。
雲は縦横半分の解像度で積分し、深度を参照して拡大合成します。遠景にも個別の立体密度を使い、空の隙間を均一な雲の層で埋めません。
雲にも既存のフィルム色調を適用します。描画負荷はウィンドウタイトルのGPU時間の`Clouds`で確認できます。

Assimpによるモデル読み込み、DirectXTexによる画像処理、RE:Dox / REDoxWrapperによるJSON処理を利用しています。
モデル・画像は `Assets` に配置しています。

- 飛行モデルはゲーム向けの近似です。
- 命中判定は機体全体を囲むOBB近似で、メッシュ単位ではありません。
- 爆発・煙・着弾はHUD描画経路による投影表示で、3D物体による遮蔽は行いません。
- 対艦ミサイルは母艦の境界への接触でダメージを与えます。爆風による周囲の機体へのダメージはありません。
- WARPへのフォールバックは機能確認用で、実機GPUと同じ性能は保証しません。

## ミッション設定

`Assets/Combat/mission.json` の `patrolCount` / `finalFighterCount` で各通常編隊の機数（1〜8）、
`bombReloadSeconds` で再装填時間（1〜120秒）、`commanderHealthMultiplier` と `enemyGunInterval` / `commanderGunInterval` で空戦難度を調整できます。
`aaRange` / `aaInterval` / `maxHostileProjectiles` は対空砲の射程、射撃間隔、敵弾の総上限です。
対艦ミサイルの弾速は `carrierMissileSpeed`（初期値6,000m/s）で設定します。
命中は配置・回転・サイズを反映した母艦全体の境界で判定します。`deckHeightFraction` / `deckWidthFraction` /
`deckLengthFraction` は母艦マーカーと予測着弾面の設定として維持します。
敵配置は各段階でプレイヤーと艦隊の位置から決定し、旧 `enemyPosition1`〜`3` は単独ミッションでは使いません。

母艦のHPは画面上部に表示します。設定の carrierHealth（初期値300）と bombDamage（初期値150）で、
対艦ミサイルの命中による耐久値を調整できます。HPが0になると母艦中央で一度だけ大爆発し、
大きな煙を出し続けます。勝敗確定後も煙は継続し、リトライでHPと演出を初期状態に戻します。

## 調整値とコードの読み方

機銃弾の寿命は自機・敵側ともに3秒です。敵側の寿命は艦砲にも適用します。
敵機銃の弾速は`mission.json`の`enemyGunSpeed`で設定し、現在は自機と同じ7,500です。
どちらも発射した機体の速度を加算するため、ワールド上での実速度は機体の速度によって変わります。
敵機は予測を弱めた照準の周囲へ、3〜6度ずらした2発を同時に発射します。
通常機は0.10秒、編隊長機は0.08秒間隔で、連射の合間には休止を挟みます。
1発のダメージは2で、発射後は等速直線運動します。
後方から発射された場合は「後方から敵機射撃！」を表示します。
通常機と編隊長機の発射間隔は
`enemyGunInterval`と`commanderGunInterval`で指定します。プレイヤー武器の発射間隔からは計算しません。
敵の照準角度と保持時間は`aiParameters.json`の`aimAngleDegrees`と`aimHoldSeconds`を使います。

AI設定の`reactionSeconds`、`maneuverSeconds`、`threatDistance`、`aimErrorDegrees`、`attackSpeed`、
`attackRunSeconds`、`flankDistance`、`maximumDisengageSeconds`、`repositionSeconds`は旧設定との互換用です。
現在の戦闘AIでは参照しません。C#との配置を維持するため残しており、変更しても戦闘中の動きは変わりません。
方位・高度の周期設定は非戦闘用の操縦処理で使用します。

実行中の対艦武器は`CarrierMissileSystem`が担当します。`BombSystem`は投下爆弾だけを扱います。
`bombReloadSeconds`、`bombDamage`、`Assets/Ammunition/Bomb/`などの既存JSONキー・パスは互換性のため維持しています。

HUDは母艦HP、飛行状態、照準、敵表示、任務表示の順に描きます。
AIは行動判断と操縦入力の計算を、モデル読み込みは形状集計とメッシュ構築を分けています。
機関砲の発射要求は`GunVolley`のメンバー名で位置・姿勢・速度・時刻を明示します。
