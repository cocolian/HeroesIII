#!/usr/bin/env python3
"""Fetch the pinned factual records used by the 0.52 data generators."""
import hashlib,json,pathlib,urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[1]
COMMIT="948333932fdf91abda780906bf2d18eec9aa0fc1"
FILES=[{'file': 'creature-ability-facts.json', 'path': 'config/spells/ability.json', 'sha256': '844d896b2c0c1b93f535a1e1afcc48248edb7685e9f298a484b9e2806350bb2c'}, {'file': 'battle-effect-facts.json', 'path': 'config/battleEffects.json', 'sha256': '7986a1a20d08be9f93cfc21d7ac5c2c8de918eaaede5b7ca05588264791a9e38'}, {'file': 'creature-casting-facts.json', 'path': 'config/spells/vcmiAbility.json', 'sha256': '1ec5f2fbdcfda25afe3b5b5241692c009fd8155d1f32fb115e6f6edfaa580702'}]
for item in FILES:
    target=ROOT/'references/video-review-0.52'/item['file']
    data=target.read_bytes() if target.exists() else urllib.request.urlopen('https://raw.githubusercontent.com/vcmi/vcmi/'+COMMIT+'/'+item['path'],timeout=60).read()
    if hashlib.sha256(data).hexdigest()!=item['sha256']:
        raise ValueError('Pinned factual record checksum mismatch: '+item['path'])
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(data)
    print(item['file'],item['sha256'])
