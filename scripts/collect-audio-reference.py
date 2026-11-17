"""Fetch only reviewed public MP3 links; never run remote programs or scripts."""
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess
import urllib.parse

root = pathlib.Path(__file__).resolve().parents[1]
catalog = json.loads((root / 'references/audio/public-downloads.json').read_text())
destination = root / 'references/audio/soundtrack'
destination.mkdir(parents=True, exist_ok=True)


def collect(item):
    output = destination / (item['key'] + '.mp3')
    metadata = output.with_suffix('.json')
    if output.exists() and metadata.exists():
        previous = json.loads(metadata.read_text())
        if previous['url'] == item['url'] and hashlib.sha256(output.read_bytes()).hexdigest() == previous['sha256']:
            return previous
    url = urllib.parse.urlparse(item['url'])
    if url.scheme != 'https' or url.hostname != 'nu.vgmtreasurechest.com':
        raise ValueError('Unreviewed media host')
    temporary = output.with_suffix('.part')
    subprocess.run(['curl', '-L', '--fail', '--silent', '--show-error', '--max-time', '60', '--max-filesize', '16777216', item['url'], '-o', str(temporary)], check=True)
    data = temporary.read_bytes()
    if len(data) > 16777216 or not (data.startswith(b'ID3') or data[:1] == b'\xff'):
        raise ValueError('Not bounded MP3 audio: ' + item['url'])
    temporary.replace(output)
    record = {**item, 'file': str(output.relative_to(root)), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'sourceType': catalog['sourceType']}
    metadata.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    print('Collected', item['key'], flush=True)
    return record


with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    records = list(pool.map(collect, catalog['tracks']))
(root / 'references/audio/soundtrack.json').write_text(json.dumps({'album': catalog['album'], 'tracks': records,
    'limitations': ['Public MP3 rip metadata is not proof of identical commercial original bytes. Demo audio has separate verified provenance.']}, ensure_ascii=False, indent=2) + '\n')
print('Collected', len(records), 'reviewed public music tracks')
