"""Read original terrain, verifying cached RoE -> SoD conversion tile for tile."""
import gzip
import hashlib
import io
import struct
from pathlib import Path
from h3m_terrain import read_terrain
ROOT=Path(__file__).resolve().parents[1]
def safe_path(relative):
    p=Path(relative)
    if p.is_absolute() or '..' in p.parts:raise ValueError('Unsafe conversion path')
    return ROOT/p

def scenario_source(item):
    source=safe_path(item['file'])
    if hashlib.sha256(source.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Original map checksum mismatch')
    conversion=item.get('conversion')
    if not conversion:return source
    target=safe_path(conversion['file'])
    if hashlib.sha256(target.read_bytes()).hexdigest()!=conversion['sha256']:raise ValueError('Converted map checksum mismatch')
    return target

def verified_terrain(item):
    source=safe_path(item['file']);target=scenario_source(item);result=read_terrain(target)
    if target!=source:
        with gzip.GzipFile(fileobj=io.BytesIO(source.read_bytes())) as f:raw=f.read(64*1024*1024+1)
        if len(raw)>64*1024*1024 or struct.unpack_from('<I',raw)[0]!=14:raise ValueError('Unexpected original RoE format')
        if struct.unpack_from('<I',raw,5)[0]!=result['size'] or raw[9]!=(result['levels']-1):raise ValueError('Converted map dimensions changed')
        blob=b''.join(bytes(t) for t in result['tiles']);offset=raw.find(blob)
        if offset<0 or raw.find(blob,offset+1)>=0:raise ValueError('RoE conversion changed original terrain records')
        result['terrainOffset']=offset
    return result
