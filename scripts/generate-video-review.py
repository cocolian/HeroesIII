#!/usr/bin/env python3
"""Catalog public video reference frames without using them as runtime art."""
import argparse, datetime, hashlib, html, json, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://www.bilibili.com/video/BV1LERPYUEoE/'

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--video', type=Path, help='Optional already downloaded public first-part 720p MP4')
    args = parser.parse_args()
    directory = ROOT / 'references/video-review-0.34'
    view = json.loads((directory / 'source/view.json').read_text())['data']
    frames = [
        (125, '冒险地图：地块、英雄、对象与道路'),
        (150, '英雄装备与部队窗口'),
        (205, '城镇中的生物属性窗口'),
        (220, '战斗：原版素材、障碍与移动范围'),
        (270, '金色行动部队与青色悬停目标'),
        (300, '战斗：受击与尸体'),
        (375, '战报窗口'),
        (665, '冒险地图中的城镇、资源与道路'),
        (900, '城镇及驻军/英雄部队'),
        (1280, '攻城场景，当前项目尚未实现'),
        (1410, '地图地形、城镇和迷雾边缘'),
    ]
    items = []
    for seconds, caption in frames:
        relative = f'frames/720p/p01-t{seconds:04}.png'
        items.append({'part': 1, 'requestedSeconds': seconds, 'caption': caption, 'file': relative,
                      'sha256': digest(directory / relative), 'kind': 'gameplay-with-enhanced-interface',
                      'videoURL': URL + f'?p=1&t={seconds}'})
    exclusions = [
        {'file': 'frames/720p/p01-t0000.png', 'reason': '作者制作的标题封面，不是游戏实机画面'},
        {'file': 'frames/preview/p17-cell020.png', 'reason': '现代风格重制/剪辑片头，非经典游戏实机；不据此推断制作技术'},
    ]
    source = {
        'recordedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'videoURL': URL,
        'title': view['title'], 'author': view['owner']['name'], 'bvid': view['bvid'],
        'parts': [{'part': p['page'], 'cid': p['cid'], 'title': p['part'], 'duration': p['duration']} for p in view['pages']],
        'totalDurationSeconds': sum(p['duration'] for p in view['pages']),
        'scope': 'First part public 720p track, key-frame sampling; thumbnail overviews of parts 5, 9 and 17. Not an exhaustive frame-by-frame review of the full series.',
        'versionAssessment': 'The recording presents Shadow of Death campaigns. Expanded windows, the combat order strip and hero side panels indicate an enhanced client/interface; exact executable and plugin versions are not verified.',
        'precisionLimit': 'Video is compressed and scaled. No claim of original native pixels, exact color calibration or same-state pixel parity.',
        'thumbnailLimit': 'Raw sheet indexes are preserved. Sheet-cell samples are previews and are not used to calibrate motion timing.',
        'runtimeArtImported': False, 'keyFrames': items, 'excludedFrames': exclusions,
        'frameFiles': {str(p.relative_to(directory)): {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in sorted((directory / 'frames').rglob('*.png'))},
        'sourceFiles': {str(p.relative_to(directory)): {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in sorted((directory / 'source').iterdir()) if p.is_file()},
    }
    if args.video:
        probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height,avg_frame_rate,duration', '-of', 'json', str(args.video)], text=True))
        source['downloadedFirstPart'] = {'bytes': args.video.stat().st_size, 'sha256': digest(args.video), 'probe': probe,
                                         'retainedInRepository': False, 'extraction': 'ffmpeg -ss SECONDS -i VIDEO -frames:v 1 OUTPUT.png'}
    (directory / 'source.json').write_text(json.dumps(source, ensure_ascii=False, indent=2) + '\n')
    escape = html.escape
    cards = ''.join(f'<figure><a href="{escape(item["file"])}"><img loading="lazy" src="{escape(item["file"])}" alt="{escape(item["caption"])}"></a><figcaption><a href="{escape(item["videoURL"])}">P1 {item["requestedSeconds"]//60:02}:{item["requestedSeconds"]%60:02}</a> · {escape(item["caption"])}</figcaption></figure>' for item in items)
    rejected = ''.join(f'<figure><img loading="lazy" src="{escape(item["file"])}"><figcaption>{escape(item["reason"])}</figcaption></figure>' for item in exclusions)
    parts = ''.join(f'<li><a href="{URL}?p={p["part"]}">{p["part"]:02} · {escape(p["title"])}</a> · {p["duration"]//60}:{p["duration"]%60:02} · <a href="source/p{p["part"]:02}-shots.json">预览索引</a></li>' for p in source['parts'])
    page = f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>0.34 视频画面对照</title>
<style>body{{background:#151a18;color:#eee4ca;font:16px/1.7 -apple-system,system-ui,sans-serif;margin:auto;max-width:1440px;padding:28px}}a{{color:#e5c471}}h1,h2{{color:#f2d48b}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}}figure{{margin:0;padding:12px;background:#252e29;border:1px solid #5b6656;border-radius:8px}}img{{display:block;width:100%;height:auto}}figcaption{{padding-top:10px}}.note{{padding:16px;border-left:4px solid #c3a45e;background:#242820}}table{{width:100%;border-collapse:collapse}}td,th{{border:1px solid #526052;padding:12px;text-align:left;vertical-align:top}}.compare img{{max-width:800px}}</style>
<h1>《死亡阴影》视频画面对照 · 0.34</h1><p><a href="{URL}">{escape(view['title'])}</a> · 作者 {escape(view['owner']['name'])} · 17 分集、约 12 小时。</p>
<p class="note">本轮抽取首集 720p 关键帧，并查看 P5/P9/P17 预览概览。画面包含高清/战斗增强界面；没有验证运行程序与插件版本。保留作者水印，录像只作对照资料，没有作为游戏素材导入。不同地图、阵营和压缩画面不计算“像素一致率”。</p>
<p><a href="source.json">来源、范围与 SHA256</a> · <a href="../../docs/video-review-0.34.md">逐项分析与实现范围</a></p>
<h2>本轮已修改</h2><table><tr><th>录像观察 / 当前问题</th><th>0.34 实现</th></tr><tr><td>P1 04:30 的当前部队为金色轮廓，悬停敌军为青色轮廓。</td><td>保留行动部队金色轮廓，新增悬停敌军青色轮廓。</td></tr><tr><td>本项目此前点击头部/身体会按上方空格处理。</td><td>点击可见精灵主体定位部队，与绘制共用动画帧、画布和镜像；透明区、阴影和尸体不拦截。</td></tr><tr><td>原先生命数值只在脚下占格处显示。</td><td>主体悬停同样显示准确生命，攻击和选取法术目标共用入口，保留 0.33 生命条。</td></tr><tr><td>录像存在障碍、攻城、行动顺序栏和详细兵种窗口。</td><td>记录为后续缺口；本轮没有把这些功能标为完成。</td></tr></table>
<h2>本项目回归场景</h2><p>下图为固定测试局面，与录像不是同一局；用于验证交互和描边。</p><div class="grid compare"><figure><img src="../validation/0.34/target-classic-hover.png"><figcaption>经典模式：金色行动部队、青色目标、主体生命提示。</figcaption></figure><figure><img src="../validation/0.34/target-classic-hit.png"><figcaption>点击目标上半身后正确攻击，生命/数量在命中时更新。</figcaption></figure></div>
<h2>实机关键帧</h2><div class="grid">{cards}</div><h2>排除的画面</h2><div class="grid">{rejected}</div><h2>分集索引</h2><p>已获取索引不代表已逐帧观看所有分集。</p><ol>{parts}</ol></html>'''
    (directory / 'index.html').write_text(page)
    print(json.dumps({'parts': len(source['parts']), 'keyFrames': len(items), 'publicReferenceSheets': len(list((directory / 'source').glob('*.jpg'))), 'firstPartSHA256': source.get('downloadedFirstPart', {}).get('sha256')}, indent=2))

if __name__ == '__main__':
    main()
