"""Keep unimplemented creature traits visible instead of treating data as behavior."""
import collections,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
units=json.load(open(root/'data/native/creatures.json'))['units']
implemented={'MORALE','LUCK','FLYING','SHOOTER','ADDITIONAL_ATTACK','ADDITIONAL_RETALIATION','UNLIMITED_RETALIATIONS','BLOCKS_RETALIATION','NO_MELEE_PENALTY','HP_REGENERATION','MANA_DRAIN','JOUSTING','CHARGE_IMMUNITY','ENEMY_DEFENCE_REDUCTION','NO_DISTANCE_PENALTY','HATE','CHANGES_SPELL_COST_FOR_ALLY','CHANGES_SPELL_COST_FOR_ENEMY'}
partial={'GARGOYLE','COMBAT_EVENT_TRIGGER','MIND_IMMUNITY','UNDEAD','NON_LIVING','MAGIC_RESISTANCE','SPELL_DAMAGE_REDUCTION','LEVEL_SPELL_IMMUNITY','SPELL_IMMUNITY','SPELL_SCHOOL_IMMUNITY','MORE_DAMAGE_FROM_SPELL'}
informational={'KING','DRAGON_NATURE','SKELETON_TRANSFORMER_TARGET'}
counts=collections.defaultdict(list)
for u in units:
 for ability in u['abilityTypes']:counts[ability].append(u['id'])
rows=[{'type':key,'state':'implemented'if key in implemented else'partial'if key in partial else'metadata-only'if key in informational else'missing','units':values}for key,values in sorted(counts.items())]
record={'scope':'Classic Complete / Shadow of Death creature abilities','meaning':{'implemented':'Behavior is connected; original-game comparison is still required.','partial':'Only effects relevant to currently supported systems are connected.','missing':'Metadata is present; game behavior is not connected.','metadata-only':'A classification used by other systems; those systems are not fully connected.'},'abilities':rows}
(root/'data/native/ability-coverage.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(dict(collections.Counter(r['state']for r in rows)))
