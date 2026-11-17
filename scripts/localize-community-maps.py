#!/usr/bin/env python3
"""Build an offline Chinese display catalog; never edit H3M rules or saves.

Seed from the matching Chinese Ina map and the VCMI Chinese terminology table.
Optional --translate uses downloaded Argos/CTranslate2 models, not a game dependency.
Reviewed corrections in reviewed-zh.json always take precedence.
"""
import argparse,csv,hashlib,json,re,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/demo/localization'
REF=ROOT/'references/localization-0.47'
FIELDS={'name','title','description','message','proposal','progress','completion','biography','bio'}
HAN=re.compile('[\u3400-\u9fff]')
FOREIGN=re.compile('[A-Za-z\u0400-\u04ff]')
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def texts(node,path=()):
    if isinstance(node,dict):
        for k,v in node.items():
            if k in FIELDS and isinstance(v,str) and v.strip():yield path+(k,),v
            elif isinstance(v,(dict,list)):yield from texts(v,path+(k,))
    elif isinstance(node,list):
        for i,v in enumerate(node):yield from texts(v,path+(i,))
def lookup(node,path):
    try:
        for k in path:node=node[k]
        return node
    except (KeyError,IndexError,TypeError):return None
def needs(s):return bool(FOREIGN.search(s)) and not HAN.search(s)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--translate',action='store_true');parser.add_argument('--models',type=Path,default=Path('/private/tmp/heroesiii-translation-models'));parser.add_argument('--opus-model',type=Path);args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    catalog=read(OUT/'zh-CN.json') if (OUT/'zh-CN.json').exists() else {'schema':1,'language':'zh-CN','common':{},'maps':{},'methods':{}}
    common=catalog['common'];terms=read(REF/'game.json')
    hero_terms={re.sub('[^a-z]','',k.split('.')[2].lower()):v for k,v in terms.items() if k.startswith('hero.core.') and k.endswith('.name') and '.specialty.' not in k}
    names=[row[0] for row in list(csv.reader((ROOT/'references/siege-rules-0.46/hotraits.txt').read_text(encoding='cp1252').splitlines(),delimiter='\t'))[2:] if row and row[0]]
    for name in names:common[name]=hero_terms[re.sub('[^a-z]','',name.lower())]
    normalize=lambda s:re.sub('[^a-z]','',s.lower())
    artifact_names={normalize(k.split('.')[2]):v for k,v in terms.items() if k.startswith('artifact.core.') and k.endswith('.name')}
    for row in csv.reader((ROOT/'references/h3m-import-0.44/artraits.txt').read_text(encoding='cp1252').splitlines(),delimiter='\t'):
        if not row:continue
        key=normalize(row[0]);value=artifact_names.get(key,artifact_names.get(key[3:]) if key.startswith('the') else None)
        if value:common[row[0]]=value
    maps={p.stem:read(p) for p in sorted((ROOT/'assets/demo/scenarios').glob('*.json'))}
    chinese=maps['ira-the-mage-1'];english=maps['ira-the-mage-3']
    def object_key(data,o):
        t=data['templates'][o['template_idx']];c=o['coordinates']
        return c['x'],c['y'],c['z'],t['object_class'],t['object_subclass']
    object_paths={object_key(chinese,o):i for i,o in enumerate(chinese['objects'])}
    entries=catalog['maps'].setdefault('ira-the-mage-3',{})
    for path,source in texts(english):
        target_path=path
        if path[0]=='objects':
            index=object_paths.get(object_key(english,english['objects'][path[1]]))
            if index is None:continue
            target_path=('objects',index)+path[2:]
        target=lookup(chinese,target_path)
        if needs(source) and isinstance(target,str) and HAN.search(target):entries[source]=target;catalog['methods']['ira-the-mage-3']='matching Chinese original at same object coordinates and display-field paths'
    reviewed=read(OUT/'reviewed-zh.json')
    common.update(reviewed['common'])
    for id,values in reviewed['maps'].items():catalog['maps'].setdefault(id,{}).update(values)
    inventory={id:sorted(set(s for _,s in texts(data) if needs(s))) for id,data in maps.items()}
    if args.translate:
        import ctranslate2,sentencepiece
        translators={}
        def load(pair):
            if pair not in translators:
                if pair=='en_zh' and args.opus_model:
                    folder=args.opus_model
                    translators[pair]=(ctranslate2.Translator(str(folder),device='cpu',compute_type='int8',inter_threads=2,intra_threads=4),sentencepiece.SentencePieceProcessor(model_file=str(folder/'source.spm')),sentencepiece.SentencePieceProcessor(model_file=str(folder/'target.spm')))
                    return translators[pair]
                folder=args.models/pair
                if not folder.exists():
                    with zipfile.ZipFile(args.models/('translate-'+pair+'-1_9.argosmodel')) as z:
                        for name in z.namelist():
                            if '..' in Path(name).parts or Path(name).is_absolute():raise ValueError('Unsafe model archive')
                        z.extractall(folder)
                model=next(folder.rglob('model.bin')).parent;sp=next(folder.rglob('sentencepiece.model'))
                tokenizer=sentencepiece.SentencePieceProcessor(model_file=str(sp))
                translators[pair]=(ctranslate2.Translator(str(model),device='cpu',compute_type='int8',inter_threads=2,intra_threads=4),tokenizer,tokenizer)
            return translators[pair]
        glossary=dict(common)
        for key,target in terms.items():
            parts=key.split('.')
            if len(parts)<4 or parts[0] not in ('artifact','spell','creatures','skill') or '.name' not in key:continue
            english=re.sub(r'([a-z])([A-Z])',r'\1 \2',parts[2]).lower()
            glossary[english]=target
            if parts[0]=='creatures':glossary[english+'s']=target;glossary[english+'es']=target
        glossary.update(reviewed.get('glossary',{}))
        glossary={k.lower():v for k,v in glossary.items() if len(k)>=3 and re.fullmatch(r"[A-Za-z][A-Za-z '\-]+",k)}
        pattern=re.compile(r'(?<![A-Za-z])('+ '|'.join(re.escape(s) for s in sorted(glossary,key=len,reverse=True))+r')(?![A-Za-z])',re.I)
        def translate(values,pair):
            translator,sp,target_sp=load(pair)
            plans=[];fragments=[]
            for source in values:
                plan=[];at=0
                for match in pattern.finditer(source) if pair=='en_zh' else []:
                    if match.start()>at:plan.append(('fragment',len(fragments)));fragments.append(source[at:match.start()])
                    plan.append(('term',glossary[match.group(0).lower()]));at=match.end()
                if at<len(source):plan.append(('fragment',len(fragments)));fragments.append(source[at:])
                plans.append(plan)
            work=[i for i,s in enumerate(fragments) if re.search('[A-Za-z\u0400-\u04ff]',s)]
            encoded=[sp.encode(fragments[i] if args.opus_model else fragments[i].lower(),out_type=str)+(['</s>'] if args.opus_model and pair=='en_zh' else []) for i in work]
            results=translator.translate_batch(encoded,beam_size=3,max_batch_size=24,max_input_length=512,max_decoding_length=384,no_repeat_ngram_size=3,repetition_penalty=1.1)
            for i,r in zip(work,results):fragments[i]=target_sp.decode(r.hypotheses[0]).replace('▁',' ').strip()
            return [''.join(value if kind=='term' else fragments[value] for kind,value in plan) for plan in plans]
        # Work sentence by sentence to preserve long stories, paragraph order and numbers.
        cache_path=REF/('machine-cache-opus.json' if args.opus_model else 'machine-cache-v2.json')
        cache=read(cache_path) if cache_path.exists() else {}
        for id in ['lord-of-the-rings-1']+[k for k in maps if k not in ('lord-of-the-rings-1','jedi-story-2')]+['jedi-story-2']:
            entries=catalog['maps'].setdefault(id,{})
            if id in ('jedi-story-2','jedi-story-3'):
                original=maps['jedi-story-1'];variant=maps[id]
                by_position={object_key(original,o):i for i,o in enumerate(original['objects'])}
                for path,source in texts(variant):
                    if not needs(source):continue
                    original_path=path
                    if path[0]=='objects':
                        index=by_position.get(object_key(variant,variant['objects'][path[1]]))
                        if index is None:continue
                        original_path=('objects',index)+path[2:]
                    english_source=lookup(original,original_path)
                    target=catalog['maps'].get('jedi-story-1',{}).get(english_source,common.get(english_source)) if isinstance(english_source,str) else None
                    if target:entries.setdefault(source,target)
            missing=[s for s in inventory[id] if s not in entries and s not in common]
            pieces={s:[p for p in re.split(r'(\n+|(?<=[.!?])\s+)',s) if p] for s in missing}
            work=sorted(set(p for ps in pieces.values() for p in ps if p.strip() and p not in cache))
            # A hard ceiling prevents any model input truncation on unpunctuated paragraphs.
            segments={s:re.findall(r'.{1,600}(?:\s+|$)|\S{1,600}',s) for s in work}
            for start in range(0,len(work),24):
                batch=work[start:start+24];flat=[part for s in batch for part in segments[s]]
                russian=[i for i,s in enumerate(flat) if re.search('[\u0400-\u04ff]',s)]
                if russian:
                    for i,value in zip(russian,translate([flat[i] for i in russian],'ru_en')):flat[i]=value
                translated=translate(flat,'en_zh');offset=0
                for s in batch:
                    count=len(segments[s]);cache[s]=''.join(translated[offset:offset+count]);offset+=count
                write(cache_path,cache)
                print(id, min(start+24,len(work)), '/',len(work),flush=True)
            for s,parts in pieces.items():entries[s]=''.join(cache[p] if p.strip() else p for p in parts)
            write(OUT/'zh-CN.json',catalog)
    # Reviewed wording wins over machine output, including terminology inside sentences.
    replacements=reviewed.get('replace',{})
    for id,entries in catalog['maps'].items():
        for source,target in list(entries.items()):
            for old,new in replacements.items():target=target.replace(old,new)
            target=target.replace('▁','').replace('⁇','…')
            target=re.sub(r'\{\\[^}]*\}','',target)
            target=re.sub('[\ue000-\uf8ff]','',target)
            for old,new in reviewed.get('postReplace',{}).items():target=re.sub(r'(?<![A-Za-z])'+re.escape(old)+r'(?![A-Za-z])',lambda m:new,target,flags=re.I)
            target=re.sub(r'(?<=[\u3400-\u9fff])Name\b','',target)
            entries[source]=target
        entries.update(reviewed['maps'].get(id,{}))
    write(OUT/'zh-CN.json',catalog)
    coverage={id:{'foreignSourceStrings':len(values),'translatedStrings':sum(s in common or s in catalog['maps'].get(id,{}) for s in values),'missing':[s for s in values if s not in common and s not in catalog['maps'].get(id,{})]} for id,values in inventory.items()}
    write(REF/'coverage.json',coverage)
    entries=dict(common)
    for id,values in catalog['maps'].items():
        for source,target in values.items():entries[id+'\n'+source]=target
    inc='// Generated by scripts/localize-community-maps.py; offline display translations.\nstatic constexpr Entry MapTextZh[]={\n'+''.join(' {'+json.dumps(s,ensure_ascii=False)+','+json.dumps(t,ensure_ascii=False)+'},\n' for s,t in sorted(entries.items()))+'};\n'
    (ROOT/'native/generated/map_text_zh.inc').write_text(inc)
    print('catalog',len(entries),'entries; missing',sum(len(v['missing']) for v in coverage.values()))
if __name__=='__main__':main()
