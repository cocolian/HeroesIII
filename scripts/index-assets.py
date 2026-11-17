import pathlib,json,struct
root=pathlib.Path(__file__).resolve().parents[1];result={}
for p in (root/'assets/original').rglob('*'):
 if p.suffix.lower()!='.png':continue
 data=p.read_bytes()[:24]
 if data[:8]!=b'\x89PNG\r\n\x1a\n':continue
 w,h=struct.unpack('>II',data[16:24]);result[str(p.relative_to(root/'assets/original')).lower()]={'url':'/'+str(p.relative_to(root)),'w':w,'h':h}
(root/'assets/manifest.json').write_text(json.dumps(result,separators=(',',':')));print(len(result),'original PNGs indexed')
