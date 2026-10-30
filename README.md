# SkyMission

**C++ / DirectX 12 を使用した、航空機と自然環境のリアルタイム3Dレンダリングプロジェクト**

SkyMissionは、C++とDirectX 12を使用して開発している3Dグラフィックスプロジェクトです。

戦闘機 F-16 と、空・雲・海洋からなる自然環境の描画を通じて、リアルタイムレンダリング、シェーダープログラミング、GPUリソース管理などのグラフィックス技術を実践的に学びながら開発しています。

また、レンダリング処理とアセット管理の責務を分離し、JSONによる設定管理や外部ライブラリとの連携を取り入れることで、拡張性と保守性を意識した設計を進めています。

## Features

### Real-Time 3D Rendering

* C++ / DirectX 12 による3Dグラフィックスの描画
* 頂点シェーダー（VS）とピクセルシェーダー（PS）を使用した描画処理
* 3Dモデルの読み込みと描画
* テクスチャを利用したマテリアル表現
* ライティングによるモデルの陰影表現

### Environment Rendering

* スカイスフィアによる空の描画
* ボリュメトリッククラウドによる雲の表現
* 法線マップを利用した海洋表面の表現
* 太陽光と環境光を考慮したライティング
* 広大な海洋環境を対象とした描画処理の調整

### Asset Management

* JSONによるアセット設定の管理
* 空・海洋・航空機の設定を用途ごとに整理
* アセットパスの解決処理をレンダラーから分離
* 専用のクラスを通じたシーンアセット情報の受け渡し

### REDox Integration

* RE:Doxを利用したJSON処理のC++プロジェクトへの統合
* C++と.NET Native AOTでビルドしたDLLとの連携
* JSONを利用したアセット設定の読み込み処理の実装

※ 一部の機能は現在も開発・改善を進めています。

## Technology Stack

| Category                   | Technology            |
| -------------------------- | --------------------- |
| Programming Language       | C++                   |
| Graphics API               | DirectX 12            |
| Shader Language            | HLSL                  |
| Development Environment    | Visual Studio 2022    |
| Configuration Format       | JSON                  |
| JSON Processing            | RE:Dox / REDoxWrapper |
| Native Library Integration | .NET Native AOT       |

## Architecture

レンダリング処理、デバイス管理、シーンアセットの読み込みなどの責務を分離し、各機能を独立して管理できる構成を目指しています。

主なコンポーネントは以下のとおりです。

| Component         | Responsibility       |
| ----------------- | -------------------- |
| Application       | アプリケーション全体の初期化と処理の管理 |
| Renderer          | 描画処理全体の管理            |
| RenderDevice      | DirectX 12のデバイス関連処理  |
| CommandContext    | コマンドリストなど、描画コマンドの管理  |
| SceneLoader       | シーンに必要なアセット情報の読み込み   |
| AssetPathResolver | JSON設定を利用したアセットパスの解決 |
| SkySphere         | 空の描画                 |
| Model             | 3Dモデルの読み込みと描画        |

※ クラス構成は開発状況に応じて変更される場合があります。

## Development Focus

### 1. Real-Time Graphics Programming

DirectX 12を使用し、GPUによる描画処理やシェーダーの実装を通じて、3Dグラフィックスの仕組みを理解しながら開発しています。

特に、航空機のマテリアル表現、ライティング、海洋表面の描画など、シーン全体の視覚的な品質向上に取り組んでいます。

### 2. Environment Rendering

空・雲・海洋を組み合わせた広大な自然環境の描画に取り組んでいます。

海洋では法線マップやライティングを利用した表面表現を、雲ではボリュメトリックな手法による立体的な表現を試行し、描画品質とパフォーマンスの両立を目指しています。

### 3. Resource Management and Performance

DirectX 12のリソース管理を意識し、テクスチャや3Dモデルの読み込み、GPUリソースの使用量、描画処理の負荷などを調査・改善しています。

### 4. Extensible Asset Pipeline

アセットのパスや設定値をJSONで管理し、アセットの読み込み処理とレンダリング処理の分離を進めています。

また、RE:Doxを利用したJSON処理をC++側へ統合し、外部ライブラリとの連携や処理性能の検証にも取り組んでいます。

## Build Environment

* Windows 10 / Windows 11
* Visual Studio 2022
* C++対応のVisual Studioワークロード
* DirectX 12に対応したGPUおよびドライバー
* Windows SDK
* プロジェクトで使用するNuGetパッケージ

## Build Instructions

1. リポジトリをクローンします。

   ```bash
   git clone https://github.com/HazukiKojima/SkyMission.git
   ```

2. Visual Studio 2022でソリューションファイルを開きます。

3. 必要なNuGetパッケージを復元します。

4. ビルド構成を選択します。

   * Configuration: `Debug` または `Release`
   * Platform: `x64`

5. プロジェクトをビルドして実行します。

※ REDoxWrapperなどの外部DLLや各種アセットが必要な構成では、それらの配置・ビルドも必要です。リポジトリの最新の構成に従ってください。

## 機関砲の操作と設定

- Fキーを押している間、プレイヤー機の4門から20mm弾を同時連射します。
- Tabで敵を切り替え、機首を向けて0.5秒捕捉するとロックが成立します。緑の予測照準に機首を近づけると小さな射撃補正が働きます。
- 弾は機体速度を引き継いで直進し、命中・寿命・基準海面到達で消滅します。敵3機を撃破でき、再出現はしません。
- `Assets/Combat/targeting.json` で捕捉条件・配置・耐久値・ダメージ、`Assets/UI/combatHud.json` でHUDの色・線幅・文字サイズを調整できます。
- ロックオンとHUDの設計は [TargetLockCombatHud.md](Docs/Design/TargetLockCombatHud.md) を参照してください。
- `Assets/Aircraft/N1K2JShidenKai/weapon.json` で発射間隔・弾速・寿命・上限を調整できます。
- 設計は [PlayerAircraftGun.md](Docs/Design/PlayerAircraftGun.md) を参照してください。

## 開発状況

SkyMissionは現在開発中です。

レンダリング機能の拡張、描画品質の改善、GPUリソース管理の見直し、アセット管理の整理などを継続して進めています。

## 入力と静的メッシュの転送

- メインウィンドウがキーボードとマウスの Raw Input を登録し、`WM_INPUT` の make/break と移動パケットから入力状態を更新します。バックグラウンド入力は登録せず、フォーカス喪失時に押下状態とマウス移動量を解除します。デバイスを外した場合も押下が残らないようにします。
- `Window::ConsumeInput` はフレームのスナップショットを返し、Application が機体とカメラへ同じ状態を渡します。押下状態は維持し、右ドラッグ中の移動量は1フレームだけ消費します。フライカメラを含め、キー・カーソル状態のポーリングは行いません。
- モデル・海・スカイスフィアの頂点とインデックスは DEFAULT ヒープに保持します。初期化時の UPLOAD バッファは Renderer がまとめて送信し、GPU 完了を待ってから解放します。定数バッファなど、CPU が更新するデータは引き続き UPLOAD ヒープを使用します。
- `Tests/RunRegressionTests.ps1` には、Raw Input の押下・解放、左右 Ctrl、複数デバイス、フォーカス喪失、入力登録の寿命、および海・空の DEFAULT ヒープと GPU 読み戻しの確認を含みます。

Raw Input の登録とメッセージ後処理は Microsoft の [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices) と [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input) の仕様に従います。

## 戦闘フィードバック

- 実際に発射した斉射で機関砲音、有効な命中で金属的な命中音を再生します。同一フレームの複数発は一音にまとめます。
- 音は初期化時に合成し、XAudio2の12音声プールで再生します。音声デバイスを利用できない場合もゲームは継続します。
- 命中位置から火花が広がり、0.45秒で消えます。最大64件を保持します。既存HUD描画経路による投影表示のため、3D物体による遮蔽は行いません。
