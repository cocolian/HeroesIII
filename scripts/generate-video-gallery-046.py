#!/usr/bin/env python3
"""Build an explicitly scoped side-by-side video evidence gallery."""
import html
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'references/video-review-0.46'
pairs=[
 ('场景选择 · 00:30','frame-0030.jpg','selection/selection-community-start-enabled.png','当前地图选择窗口仍与原版不同。'),
 ('英雄窗口 · 01:00','frame-0060.jpg','siege/video-hero-opening.png','视频是 Gelare，当前是开局 Lacus。比较结构、调色板和头像，不比较兵力。'),
 ('元素城攻城 · 15:00','frame-0900.jpg','siege/siege-8-3.png','背景与城防组件已接入。两图部队、城防级别和游戏时刻不同，不是等价状态。'),
 ('胜利结算 · 29:20','frame-1760.jpg','siege/video-map-victory.png','视频 19 天获胜，当前回放 35 天。尚缺原版积分榜和结算动画。')]
head='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>0.46 视频与攻城对照</title>
<style>body{margin:0;background:#151511;color:#eee8d2;font:16px/1.6 system-ui}main{max-width:1600px;margin:auto;padding:30px}h1{font-size:28px}h2{font-size:21px;color:#edd788}.pair,.frames{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}figure{margin:0}img{width:100%;object-fit:contain;background:#080806}figcaption{color:#c4bba1}section{margin:32px 0;padding:22px;background:#23231b}a{color:#ecc36c}.frames{grid-template-columns:repeat(3,minmax(0,1fr))}@media(max-width:800px){.pair,.frames{grid-template-columns:1fr}}</style>
<main><h1>0.46 原视频、地图与当前画面</h1><p>来源：<a href="https://www.bilibili.com/video/BV1vu41177kZ/">奔跑的脚丫RunningFoot · 死人和被埋葬的人</a>。视频使用 SoD / HD mod 与中文本地化。当前导入俄文版官方 RoE 地图，原地形和对象身份已核对。</p><p><strong>尚未达到逐屏或逐像素一致。</strong>下图用于观察构图、素材与尚存差异。随机开局、游戏时刻和兵力不同，不计算整屏相似率。<a href="../../docs/video-siege-campaign-0.46.md">实测报告</a> · <a href="source.json">来源与时间点</a></p>'''
parts=[head]
for title,video,engine,note in pairs:
    parts.append('<section><h2>'+html.escape(title)+'</h2><p>'+html.escape(note)+'</p><div class="pair"><figure><img loading="lazy" src="'+video+'"><figcaption>原视频帧</figcaption></figure><figure><img loading="lazy" src="../validation/0.46/'+engine+'"><figcaption>0.46 引擎截图</figcaption></figure></div></section>')
parts.append('<section><h2>完整抽帧索引</h2><div class="frames">')
for p in sorted(OUT.glob('frame-*.jpg')):
    seconds=int(p.stem.split('-')[1])
    parts.append('<figure><a href="'+p.name+'"><img loading="lazy" src="'+p.name+'"></a><figcaption>'+f'{seconds//60:02}:{seconds%60:02}'+'</figcaption></figure>')
parts.append('</div></section></main></html>')
(OUT/'index.html').write_text(''.join(parts))
print('Generated video comparison gallery')
