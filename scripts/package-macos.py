#!/usr/bin/env python3
"""Create a self-contained, locally signed Mac app, DMG and Installer package."""
import argparse,hashlib,json,os,plistlib,re,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args,**kwargs):return subprocess.run([str(x) for x in args],check=True,**kwargs)
def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=ROOT/'build/distribution');p.add_argument('--output',type=Path,default=ROOT/'dist');p.add_argument('--no-dmg',action='store_true');a=p.parse_args()
    source=a.build/'HeroesIII.app';info=plistlib.loads((source/'Contents/Info.plist').read_bytes());version=info['CFBundleShortVersionString']
    data=source/'Contents/Resources/GameData';assert (data/'runtime-manifest.json').is_file(),'Embed staged GameData before packaging'
    binary=source/'Contents/MacOS/HeroesIII';deps=subprocess.check_output(['otool','-L',str(binary)],text=True)
    assert '/opt/homebrew' not in deps and '@rpath' not in deps,'Distribution still needs external dylibs'
    a.output.mkdir(parents=True,exist_ok=True);stage=a.build/'installer-stage';stage.mkdir(exist_ok=True);app=stage/'HeroesIII.app'
    if app.exists():shutil.rmtree(app)
    run(['/usr/bin/ditto',source,app]);run(['/usr/bin/codesign','--verify','--strict',app])
    link=stage/'Applications'
    if not link.exists():link.symlink_to('/Applications',target_is_directory=True)
    (stage/'安装说明.txt').write_text(f'''Heroes III {version} · C++ 原生开发测试版\n\n将 HeroesIII.app 拖到 Applications，然后从应用程序打开。\n支持 Apple Silicon 与 Intel，最低 macOS 14；支持 Retina 与 F4 全屏。\n所有运行素材和静态依赖均在应用内，不需要 Homebrew 或本工程。\n用户设置与游戏进度保存在 ~/Library/Application Support/HeroesIII/。\n这是本地临时签名的测试包，尚无 Developer ID 签名或 Apple 公证。\nSeven Lakes 已完成简单难度、阿德拉开局的正常流程通关验证；保留正常读档重试记录。完整原版功能与画面仍在开发中。\n此 Mac 包不能安装到 iPad。iPad 必须用 iPadOS 构建并经过苹果签名。\n''')
    with (stage/'安装说明.txt').open('a') as f:
        f.write('15 张下载地图可在“新游戏 → 选择地图 · 已下载地图”进入；已加入多英雄、电脑经营与基础攻城；复杂剧情、部分英雄／生物特长及逐帧画面对齐仍需继续验证。\n')
    names=[]
    if not a.no_dmg:
        dmg=a.output/f'HeroesIII-{version}-macOS-universal.dmg'
        run(['/usr/bin/hdiutil','create','-volname',f'Heroes III {version}','-srcfolder',stage,'-format','UDZO','-ov',dmg]);names.append(dmg)
    with tempfile.TemporaryDirectory(prefix='heroesiii-installer-') as tmp:
        payload=Path(tmp)/'payload';target=payload/'Applications/HeroesIII.app';target.parent.mkdir(parents=True)
        run(['/usr/bin/ditto',app,target]);components=Path(tmp)/'components.plist'
        run(['/usr/bin/pkgbuild','--analyze','--root',payload,components]);records=plistlib.loads(components.read_bytes())
        for item in records:item['BundleIsRelocatable']=False;item['BundleOverwriteAction']='upgrade'
        components.write_bytes(plistlib.dumps(records));pkg=a.output/f'HeroesIII-{version}-macOS-universal.pkg'
        run(['/usr/bin/pkgbuild','--root',payload,'--component-plist',components,'--identifier','local.heroesiii.native.pkg','--version',version,'--install-location','/',pkg]);names.append(pkg)
    record={'version':version,'architectures':subprocess.check_output(['lipo','-archs',str(binary)],text=True).strip().split(),'minimumMacOS':info['LSMinimumSystemVersion'],'selfContained':True,'sourceFallbackEnabled':False,'developerIdSigned':False,'appleNotarized':False,'files':{x.name:{'bytes':x.stat().st_size,'sha256':hashlib.sha256(x.read_bytes()).hexdigest()} for x in names}}
    (a.output/'macos-package.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');print(json.dumps(record,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
