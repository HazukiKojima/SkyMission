# 開始時の敵編隊紹介

ゲーム開始と再挑戦で約5秒の導入を再生する。Enterでスキップできる。
0〜3.6秒はCorsair 4機を斜め前から撮影し、3.6秒から暗転、3.8秒で戦闘シーンへ切り替える。
4秒から通常追従視点へフェードインし、5秒から操作を受け付ける。

## 調整

`Assets/Cinematics/opening.json`を編集して再起動する。ファイルや各項目がなければ既定値を使う。
型不一致・不正な数値・壊れたJSONは無視せず、ファイル名と項目を含むエラーにする。

| 項目 | 用途 |
| --- | --- |
| durationSeconds | 導入全体の秒数 |
| fadeOutStartSeconds / cutSeconds | 暗転開始と描画シーン切替の時刻 |
| fadeInStartSeconds / fadeInSeconds | 明転開始時刻と所要時間 |
| formationSpacingMeters / formationDepthMeters | 僚機の左右間隔と先頭機からの後方距離 |
| flightSpeedMetersPerSecond | 演出用の直進速度 |
| propellerRadiansPerSecond | 演出用のプロペラ回転速度 |
| cameraStartOffsetMeters / cameraEndOffsetMeters | 先頭機中心から見たカメラの開始・終了位置 |
| lookAtOffsetMeters / verticalFovDegrees | 注視点のオフセットと縦視野角 |
| title / subtitle | 紹介文。既存HUDフォントに収録した文字と英数字を使用 |

敵機はワールド+Z方向へ飛ぶ。カメラ位置・編隊間隔・注視点は最大寸法200mの機体を基準にし、
モデルの最大寸法とvisualScaleに合わせて一括で拡縮する。機体の中心はローカル境界から求める。
時刻は `0 < fadeOutStartSeconds < cutSeconds <= fadeInStartSeconds`、
`fadeInStartSeconds + fadeInSeconds <= durationSeconds` を満たす必要がある。

## 責務とライフサイクル

- Settingsとローダーは既定値・JSON読み込み・検証を担当する。
- Controllerは経過時間からカット、フェード、並走カメラ、直進距離を求める。入力機器や描画には依存しない。
- OpeningCinematicSceneは共有モデルを使う4機と専用カメラを所有する。戦闘オブジェクトは変更しない。
- FlightSceneは導入と戦闘を切り替える。Applicationだけが本番で導入を有効化し、個別テストは直接戦闘を開始できる。
- OpeningCinematicPresenterは値コピーの表示データを受け取り、黒帯・紹介文・フェード・スキップ案内を描く。

開始時に通常追従カメラを確定させ、敵編隊のカット中は戦闘シーンを停止する。
自機カットへ切り替えた後は無入力の機体移動とカメラ追従を更新し、操作開始まで映像を止めない。
武器・ダメージ・任務の更新と操作入力は終了フレームまで無効にする。スキップでは追加の飛行を行わない。
再挑戦は既存の初期状態を復元した後で導入時間を0に戻す。最小化中は既存のメインループで更新を停止する。
演出シーンと戦闘シーンの両方へリサイズを通知する。飛行音には既存のループ音を使用する。

## 検証

`Tests/RunRegressionTests.ps1 -MissionReview`で設定・状態遷移と既存の武器・任務・HUDを検証する。
`Tests/RunRegressionTests.ps1 -OpeningReview`で実モデルの描画キャプチャも生成する。
出力先のfixturesに、開始・3秒・横長・800x600・暗転・自機視点・操作開始のPNGを保存する。
