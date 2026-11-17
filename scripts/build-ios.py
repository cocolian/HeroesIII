#!/usr/bin/env python3
"""Build the shared C++ app for an iPad, simulator, or Xcode archive."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args,**kwargs):return subprocess.run(args,check=True,**kwargs)
def preflight(env):
    result=subprocess.run(['xcrun','--sdk','iphoneos','--show-sdk-path'],env=env,text=True,capture_output=True)
    identities=subprocess.run(['security','find-identity','-v','-p','codesigning'],env=env,text=True,capture_output=True)
    return {'iphoneosSdkAvailable':result.returncode==0,'sdkPath':result.stdout.strip() if result.returncode==0 else None,'signingIdentityAvailable':'0 valid identities found' not in identities.stdout and identities.returncode==0,'xcodeDownload':'https://developer.apple.com/download/all/?q=Xcode%2026.6','appleDistributionGuide':'https://developer.apple.com/documentation/xcode/distributing-your-app-to-registered-devices'}
def verify_runtime(app):
    data=app/'GameData'
    manifest=json.loads((data/'runtime-manifest.json').read_text())
    for name,expected in manifest['files'].items():
        path=data/name
        if not path.is_file() or path.stat().st_size!=expected['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest()!=expected['sha256']:
            raise RuntimeError(f'iPad bundle runtime verification failed: {name}')
    if not (app/'Assets.car').is_file():raise RuntimeError('iPad app icon catalog is missing')
    run(['codesign','--verify','--strict',str(app)])
    print(json.dumps({'app':str(app),'bundledRuntimeFilesVerified':len(manifest['files']),'iconCatalogAvailable':True},ensure_ascii=False),flush=True)
def main():
    p=argparse.ArgumentParser();p.add_argument('--preflight',action='store_true');p.add_argument('--developer-dir');p.add_argument('--team');p.add_argument('--simulator',action='store_true');p.add_argument('--archive',action='store_true');p.add_argument('--export-options',type=Path);p.add_argument('--output',type=Path,default=ROOT/'dist/ipad');a=p.parse_args();env=dict(os.environ)
    if a.developer_dir:env['DEVELOPER_DIR']=a.developer_dir
    check=preflight(env)
    if a.preflight:print(json.dumps(check,ensure_ascii=False,indent=2));return 0
    if not check['iphoneosSdkAvailable']:
        print('尚未找到完整 Xcode 的 iOS SDK。请安装兼容当前 macOS 的 Xcode、首次打开完成安装，然后用 --developer-dir 指向 Xcode.app/Contents/Developer。',file=sys.stderr);return 2
    if not a.simulator and not a.team:
        print('真机安装/归档需要 --team APPLE_DEVELOPMENT_TEAM；在 Xcode 登录 Apple 账号后可获取。',file=sys.stderr);return 2
    staging=ROOT/'build/staged-runtime/GameData'
    run([sys.executable,str(ROOT/'scripts/stage-runtime.py'),str(staging)],env=env)
    build=ROOT/'build'/('ipad-simulator' if a.simulator else 'ipad-device');sdk='iphonesimulator' if a.simulator else 'iphoneos'
    cmd=['cmake','-S',str(ROOT),'-B',str(build),'-G','Xcode','-DCMAKE_SYSTEM_NAME=iOS',f'-DCMAKE_OSX_SYSROOT={sdk}','-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0','-DH3_VENDOR_DEPENDENCIES=ON','-DH3_USE_SOURCE_ASSETS=OFF',f'-DH3_RUNTIME_DATA_DIR={staging}','-DBUILD_TESTING=OFF']
    if a.team:cmd.append(f'-DH3_DEVELOPMENT_TEAM={a.team}')
    run(cmd,env=env)
    if a.archive:
        a.output.mkdir(parents=True,exist_ok=True);archive=a.output/'HeroesIII.xcarchive'
        run(['xcodebuild','-project',str(build/'HeroesIII.xcodeproj'),'-scheme','HeroesIII','-configuration','Release','-destination','generic/platform=iOS','-archivePath',str(archive),'archive','-allowProvisioningUpdates'],env=env)
        verify_runtime(archive/'Products/Applications/HeroesIII.app')
        if a.export_options:run(['xcodebuild','-exportArchive','-archivePath',str(archive),'-exportPath',str(a.output),'-exportOptionsPlist',str(a.export_options.resolve()),'-allowProvisioningUpdates'],env=env)
    else:
        run(['cmake','--build',str(build),'--config','Release','--target','HeroesIII'],env=env)
        verify_runtime(build/f'Release-{sdk}/HeroesIII.app')
    return 0
if __name__=='__main__':sys.exit(main())
