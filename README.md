# PticketGetter

Windows x64 向けの C++ シューティングミニゲームです。  
上から降ってくるチケットを回収して、自機を強化しながらスコアを伸ばします。

## 現在の実装範囲
- Win32 + GDI+ で動作する単体アプリ
- 縦画面レイアウト（設計サイズ: `720 x 1280`）
- シューティングゲームループ
  - 自機は常時オートショット
  - 敵を撃破してスコア獲得
  - 上から降るチケットを回収
- ステージ進行
  - 全10ステージ
  - 各ステージ終盤でボス戦
  - ボス撃破後にショップを開いて次ステージへ遷移
  - ステージクリア時にHPを全回復
- ポーズ
  - `P` キーでポーズ / 再開
- チケット強化
  - 回収したチケットは `P` カウンタに加算
  - チケット数に応じて `POWER Lv.` 上昇
  - 弾数・連射性能・移動性能が強化
- BGM再生
  - `assets/BGM/stage1.mp3` 〜 `stage10.mp3` があればステージごとにループ再生
  - なければ `assets/BGM/bgm_loop.mp3` / `bgm_loop.wav` をループ再生
  - `M` キーでON/OFF

## 依存関係
- OS: Windows 11 x64
- ビルドツール: Visual Studio Code (MSVC v143 / Desktop C++ / Windows SDK)
- CMake: 3.20 以上
- リンクするWindows標準ライブラリ: `gdiplus`, `msimg32`, `winmm`

配布先PCで実行するだけの場合は、`Microsoft Visual C++ 2015-2022 Redistributable (x64)` が必要になる場合があります。

## 素材配置
以下の素材を配置してください。

- `assets/p_icon.png` (または `assets/point.png` / `assets/1.png`)  
  チケット総数カウンタ用アイコン
- `assets/ticket_icon.png` (または `assets/ticket.png` / `assets/3.png`)  
  落下チケット表示用アイコン
- `assets/BGM/bgm_loop.wav`  
  ループ再生BGM（ステージ別BGM未配置時のフォールバック）
- `assets/BGM/stage1.mp3` ～ `assets/BGM/stage10.mp3`  
  ステージ別BGM（推奨、`stage01.mp3` / `bgm_stage1.mp3` / `.wav` も対応）

アイコンが見つからない項目はフォールバック描画で表示されます。

## ビルド手順 (Visual Studio / x64)
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

またはプリセット版:
```powershell
cmake --preset vs2022-x64
cmake --build --preset release-x64
```

このプロジェクトは x64 専用です。`Win32` など 32bit ターゲットで構成すると CMake 段階でエラーになります。

実行ファイル:
- `build/Release/PticketGetter.exe`

## 操作
- キーボード
  - `W` `A` `S` `D`: 移動
  - `↑` `↓` `←` `→`: 移動
  - `Shift` + 移動: 低速移動
  - `P`: ポーズ / 再開
  - `R`: いつでもリトライ
  - `1` `2`: ショップで購入
  - `Enter` / `Space`: ショップを閉じて次ステージへ
  - `M`: BGM ON/OFF
  - `Esc`: 終了

## フォルダ構成
```text
.
├─ CMakeLists.txt
├─ README.md
├─ assets/
└─ src/
   ├─ asset_catalog.cpp / .h
   ├─ game_app.cpp / .h
   ├─ game_types.h
   ├─ shooter_game.cpp / .h
   ├─ stage_book.cpp / .h
   └─ main.cpp
```
