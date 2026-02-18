BGM配置ルール（PticketGetter）

このフォルダにBGMファイルを配置してください。
対応形式: `.mp3` / `.wav`

推奨構成（N = 1..10）:

1) ステージ通常BGM
- `stageN.mp3`
- `stage0N.mp3`
- `bgm_stageN.mp3`
- `N.mp3`

2) ボスBGM（任意）
見つからない場合は、同ステージの通常BGMを使います。
- `boss_stageN.mp3`
- `boss_stage0N.mp3`
- `stageN_boss.mp3`
- `stage0N_boss.mp3`
- `bossN.mp3`
- `boss0N.mp3`
- `boss/stageN.mp3`
- `boss/stage0N.mp3`

3) 全体フォールバックBGM
ステージ個別BGMが見つからない場合に使います。
- `bgm_loop.mp3`
- `bgm_loop.wav`

補足:
- 上記パターンは `.wav` でも同様に有効です。
- 独自ファイル名を使う場合は、どれかの命名規則に合わせてリネームしてください。
