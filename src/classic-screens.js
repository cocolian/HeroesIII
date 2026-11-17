import * as Art from './art.js';
export function drawHero(canvas,state){const c=canvas.getContext('2d'),h=state.hero;c.clearRect(0,0,665,580);Art.draw(c,'reference/hero',0,0,665,580,[70,10,665,580],false);c.imageSmoothingEnabled=false;
 const panel=(x,y,w,hh)=>{c.fillStyle='#38291b';c.fillRect(x,y,w,hh);c.strokeStyle='#8c744c';c.lineWidth=1;c.strokeRect(x+.5,y+.5,w-1,hh-1);};
 const text=(v,x,y,size=13,color='#f1dca3')=>{c.font=`${size}px Georgia,"Songti SC",serif`;c.fillStyle=color;c.textAlign='center';c.fillText(String(v),x,y);};
 Art.draw(c,'hero-portraits/HeroPortraits.png',13,18,58,64,[68,635,58,64]);panel(76,19,218,61);text('凯瑟琳',185,47,20);text(`${h.level} 级 骑士`,185,69,15);
 panel(13,153,282,19);[h.attack,h.defense,h.power,h.knowledge].forEach((n,i)=>text(n,47+i*70,168));
 panel(59,177,90,47);text('骑士',104,197,13);text('城堡',104,216,12);panel(58,231,91,42);text('经验',103,246,12);text(`${h.xp} / ${h.level*1000}`,103,263,11);panel(200,231,94,42);text('魔法值',247,246,12);text(`${h.mana} / ${h.maxMana}`,247,263,13);
 panel(12,274,283,195);text('英雄日志',153,303,16);text(`等级 ${h.level} · 行动力 ${h.movement}`,153,332,13);text('已学魔法',153,366,14);text(h.spells.includes('heal')?'魔法神箭 · 治疗术':'魔法神箭',153,391,13);text('宝物',153,423,14);text(h.artifacts.join('、')||'尚未获得宝物',153,447,12);
 // Empty original artifact slots, retaining the original paper-doll silhouette.
 for(const [x,y,w,hh] of [[312,58,47,47],[362,58,47,47],[437,20,48,48],[490,20,48,48],[540,20,48,48],[490,73,48,48],[312,135,47,47],[438,113,48,48],[540,118,48,48],[320,184,47,47],[465,176,48,48],[531,176,48,48],[312,237,47,47],[490,230,48,48],[310,286,47,47],[362,286,47,47],[540,292,48,48],[438,291,48,48],[339,355,48,48],[389,355,48,48],[439,355,48,48],[489,355,48,48],[539,355,48,48],[438,73,48,48],[490,176,48,48],[337,237,48,48]])panel(x,y,w,hh);
 if(h.artifacts.length)text('⚔',385,323,36);
 panel(606,63,51,385);Art.draw(c,'hero-portraits/HeroPortraits.png',609,80,45,50,[68,635,58,64]);
 for(let i=0;i<7;i++){panel(11+i*65,482,59,70);const a=h.army[i];if(a){Art.draw(c,'creature-portraits/CreaturePortraits.png',12+i*65,483,57,63,Art.unitRects[a.unit]);text(a.count,42+i*65,548,15);}}
 panel(11,554,576,19);text(`攻击 ${h.attack}　防御 ${h.defense}　力量 ${h.power}　知识 ${h.knowledge}`,300,568,11);
}
