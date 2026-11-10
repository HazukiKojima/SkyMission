# ゲーム設定のJSON化

ゲームの調整値をJSONから起動時に読み込む。変更の反映にはアプリの再起動が必要。
追加した項目と設定ファイルは省略可能で、省略時は従来の初期値を使う。
既存JSONの必須項目の扱いは変更しない。オープニングの文字倍率は1.5、それ以外は1.0。

## 設定ファイル

| ファイル（Assets配下） | 設定内容 |
| --- | --- |
| `Combat/mission.json` | 自機開始位置、戦闘区域・接近距離、編隊間隔・前後差・高度差、初動の遅延、対空砲の弾速・威力・予測時間・照準のばらつき・発射位置、甲板の厚さ、任務メッセージ表示時間 |
| `Combat/enemyWeapon.json` | 敵機銃の威力、弾の寿命、初回の待ち時間、連射・休止時間とばらつき、予測射撃の割合、照準誤差、散布角 |
| `Aircraft/F4U4Corsair/aiParameters.json` | 既存AI設定に加え、離脱時間、防衛半径、連続旋回時間、後方判定、非ロック機の攻撃性倍率、経路・再進入位置・旋回方向、機体別行動パターン |
| `Sound/audio.json` | 飛行音・機銃・ミサイル・被弾・爆発・戦闘BGMのファイル名と音量、警告音の音量と繰り返し間隔 |
| `Effects/combatEffects.json` | `gun`・`impact`・`explosion`・`carrier`・`rocket`・`deck`ごとの寿命、半径、太さ、不透明度、移動・上昇速度、発生間隔、破片や煙のサンプル数 |
| `Camera/aircraftCamera.json` | 通常カメラのFOV、移動速度、マウス感度、追従距離・高さ・補間、見回し復帰、ピッチ制限、高速時の距離変化、注視点の先行距離 |
| `UI/combatHud.json` | 既存HUD設定に加え、レーダー範囲、レーダー・演出の基本色、選択・警告・注意のRGBA色 |
| `UI/labels.json` | 操作説明、任務文、警告文、敵の表示名。`labels`内の元の日本語をキーとして、表示する文字列を指定 |
| `Cinematics/*.json` | `textScale`、`briefingFontSize`、`titleFontSize`、`subtitleFontSize`、`hintFontSize`、`lineSpacing`、`barHeightFraction`、`titleBottomOffset`、`subtitleBottomOffset`、`skipHint` |
| `Ammunition/Rocket/ammunition.json` | ミサイルの誘導旋回速度、左右・下方向の発射オフセット、寿命、煙の発生間隔、搭載数 |

ミサイル特性は、弾薬定義の`model`と同じディレクトリにある`ammunition.json`から読む。
既存の`Ammunition/Bomb/ammunition.json`はRocketモデルを参照するため、Rocket側を編集する。
弾速・威力・再装填時間は引き続き`Combat/mission.json`で設定する。

## 単位と制約

- 時間は秒、距離は既存のワールド単位。角度はキーが`Degrees`なら度、`Radians`ならラジアン。
- UIの文字サイズ・オフセット・線幅は1920×1080を基準に画面サイズで縮尺する。
- 音量・RGBA・速度継承率は0〜1。レーダー範囲・寿命・補間速度は正数。
- `textScale`は0より大きく4以下。長い字幕や紹介文は既存の画面幅制限に合わせて縮小する。
- `depthOffsetsMeters`と`altitudeOffsetsMeters`は各6要素。AIの経路・行動パターン配列も現在の要素数を維持する。
- ミサイル搭載数は1〜16の整数、寿命は0.001〜120秒。搭載数と寿命を使って描画モデルの予約数も計算する。発射位置は左右交互。
- エフェクトの煙・炎・破片のサンプル数は、既存の描画予算内で1から既定数まで変更できる。生存個数上限と頂点予算はコードで管理する。
- 表示文字列は1〜192 UTF-8バイト。描画可能な日本語は既存の`HudFont.h`の字形集合に従う。文字列設定だけでは字形は増えない。
- 操作説明の文言変更ではキー割り当ては変わらない。

範囲外、型違い、配列の要素数違い、大小関係の不整合は読み込み時に例外にし、ファイル名と項目名を出す。
設定は各シーン・カメラ・音声・Presenterが保持し、別インスタンスへ波及させない。
リトライでは開始時のシーン状態とともに設定を復元する。

## 実装上の境界

GPUバインド番号、物理計算の時間刻み、ゼロ除算回避値、ハッシュ定数、頂点予算・容量上限はコードに残す。
AIの追加設定は既存のREDox連携構造体の外に保持し、ネイティブとC#間のレイアウトを変えない。

## 検証

`Tests/GameplaySettingsTests.h`で省略時の初期値、不正値の拒否、配列、表示文字列、演出文字の受け渡し、
エフェクト寿命、レーダー範囲、ミサイル容量・発射位置、射撃時間、インスタンス間の独立性を検証する。
`Tests/MissionTests.h`では本番のシーン生成経路で自機開始位置とミサイル搭載数の反映を確認する。

```powershell
./Tests/RunRegressionTests.ps1 -BuildDirectory out/json-tuning-regression `
  -RedoxDll External/REDoxWrapper/bin/Release/net10.0/win-x64/publish/REDoxWrapper.dll
```
