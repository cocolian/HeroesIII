import concurrent.futures,pathlib,subprocess,json,hashlib,sys
root=pathlib.Path(__file__).resolve().parents[1];dest=root/'references/rules/wiki';dest.mkdir(parents=True,exist_ok=True)
names=['Castle','Rampart','Tower','Inferno','Necropolis','Dungeon','Stronghold','Fortress','Conflux','List_of_creatures','Horde_building','Resource_Silo','Stables','Mage_Guild','Growth','Artifact_Merchants','Ballista_Yard','Halls','Fortifications','Blacksmith','Marketplace','Tavern','Behemoth_and_Ancient_Behemoth','Mage_and_Arch_Mage','Pegasus_and_Silver_Pegasus','Unicorn_and_War_Unicorn','Sharpshooter','Damage','Undead','Aura_of_Magic_Resistance','Magic_Arrow','Lightning_Bolt','Cure','Haste','Slow','Shield','Stone_Skin','Bless','Curse','Magic_University','List_of_spells','Ice_Bolt','Implosion','Chain_Lightning','Frost_Ring','Fireball','Inferno_(spell)','Meteor_Shower','Death_Ripple','Destroy_Undead','Armageddon','Titan%27s_Lightning_Bolt','Air_Shield','Fire_Shield','Protection_from_Air','Protection_from_Fire','Protection_from_Water','Protection_from_Earth','Anti-Magic','Bloodlust','Precision','Weakness','Disrupting_Ray','Prayer','Counterstrike','Forgetfulness','Dispel']
from spell_catalog import catalog
names=list(dict.fromkeys(names+['Trading_Post','Wisdom','Library','Spell_Book','Resurrection','Animate_Dead','Sacrifice','Clone','Summon_Fire_Elemental','Summon_Earth_Elemental','Summon_Water_Elemental','Summon_Air_Elemental']+[row['page']for row in catalog().values()if(dest/(row['page']+'.html')).exists()]))
def get(name):
 url='https://heroes.thelazy.net/index.php/'+name
 path=dest/(name+'.html')
 data=path.read_bytes()if path.exists()and'--refresh'not in sys.argv else subprocess.check_output(['curl','-sSL','--fail','--retry','1','--max-time','90',url]);path.write_bytes(data);return {'page':name,'url':url,'sha256':hashlib.sha256(data).hexdigest()}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:rows=list(pool.map(get,names))
(root/'references/rules/wiki-sources.json').write_text(json.dumps(rows,indent=2));print('Downloaded',len(rows),'factual stat/town references')
