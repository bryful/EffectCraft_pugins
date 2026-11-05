# Smooth for EffectCraft

セル画や塗り分け素材の輪郭を滑らかにする EffectCraft デスクトップ・CLI 用プラグインです。[loilo-inc/smooth](https://github.com/loilo-inc/smooth/tree/3f38523abddf0317f39eafa320f9de389334cb5d) の角、突起、欠けの処理を EffectCraft の Wasm プラグイン API v1 に移植しています。

## ビルドと読み込み

Zig を PATH に追加し、PowerShell で実行します。別の場所にある場合は `-Zig` で実行ファイルを指定できます。ビルド確認には Zig `0.12.0-dev.1286+cc394431a` を使用しました。

```powershell
.\build.ps1
```

EffectCraft で **Effect → Load Effect Plug-in…** を選び、`dist/smooth.wasm` を読み込みます。**Effects & Presets → Stylize → Smooth** からレイヤーに適用できます。CLI でのエフェクト ID は `org.effectcraft.community.smooth` です。

## パラメーター

| 項目 | 動作 | 初期値 |
| --- | --- | --- |
| White Transparent | RGB が純白の画素を透明にしてから輪郭を処理 | オフ |
| Range | 同じ色とみなす RGBA の色差のしきい値（0–100） | 1 |
| Line Weight | 線の太さを調整（0–1） | 0 |

輪郭の判定と補間は元の 8 bpc 経路を使用します。EffectCraft のプリマルチプライド RGBA を処理用に変換し、変化した画素だけを書き戻します。処理対象となった HDR 画素は 8 bpc の範囲に収まります。縮小プレビューでは縮小後の画素を処理します。

## ライセンス

[Apache-2.0](LICENSE)。元の smooth から移植したファイルは先頭に移植元と変更を記載しています。
