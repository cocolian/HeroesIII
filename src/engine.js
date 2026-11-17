export const WIDTH=36, HEIGHT=28;
export const BATTLE_WIDTH=15, BATTLE_HEIGHT=11;
export const UNITS={
 pikeman:{name:'枪兵',icon:'♟',hp:10,attack:4,defense:5,min:1,max:3,speed:4,cost:60,growth:14},
 archer:{name:'弓箭手',icon:'♙',hp:10,attack:6,defense:3,min:2,max:3,speed:4,cost:100,growth:9,ranged:true},
 griffin:{name:'狮鹫',icon:'♞',hp:25,attack:8,defense:8,min:3,max:6,speed:6,cost:200,growth:7,flying:true},
 swordsman:{name:'剑士',icon:'♜',hp:35,attack:10,defense:12,min:6,max:9,speed:5,cost:300,growth:4},
 monk:{name:'僧侣',icon:'♗',hp:30,attack:12,defense:7,min:10,max:12,speed:5,cost:400,growth:3,ranged:true},
 cavalier:{name:'骑士',icon:'♘',hp:100,attack:15,defense:15,min:15,max:25,speed:7,cost:1000,growth:2},
 angel:{name:'天使',icon:'♕',hp:200,attack:20,defense:20,min:50,max:50,speed:12,cost:3000,growth:1,flying:true},
 skeleton:{name:'骷髅兵',icon:'♟',hp:6,attack:5,defense:4,min:1,max:3,speed:4,cost:60},
 wolf:{name:'恶狼',icon:'♞',hp:10,attack:7,defense:5,min:2,max:4,speed:6,cost:100},
 orc:{name:'半兽人',icon:'♜',hp:15,attack:8,defense:4,min:2,max:5,speed:4,cost:150,ranged:true}
};
export const BUILDINGS={
 hall:{name:'议会',icon:'♜',desc:'城镇每日收入增加 1,000 金币',cost:{gold:2500},requires:[]},
 barracks:{name:'兵营',icon:'⚔',desc:'每周可招募 4 名剑士',cost:{gold:2000,wood:5,ore:5},requires:[] ,unit:'swordsman'},
 tower:{name:'狮鹫塔',icon:'♞',desc:'每周可招募 7 只狮鹫',cost:{gold:1000,ore:5},requires:[],unit:'griffin'},
 mage:{name:'一级魔法行会',icon:'✧',desc:'学会魔法神箭、治疗术；每日回满魔法',cost:{gold:2000,wood:5,ore:5},requires:[]},
 monastery:{name:'修道院',icon:'♗',desc:'每周可招募 3 名僧侣',cost:{gold:3000,wood:5,ore:5,crystal:2},requires:['barracks','mage'],unit:'monk'},
 stable:{name:'骑兵营',icon:'♘',desc:'每周可招募 2 名骑士',cost:{gold:5000,wood:10},requires:['barracks'],unit:'cavalier'},
 portal:{name:'天使之门',icon:'♕',desc:'每周可招募 1 名天使',cost:{gold:12000,ore:10,crystal:5,gem:5},requires:['monastery','stable'],unit:'angel'}
};
export const RESOURCE_NAMES={gold:'金币',wood:'木材',ore:'矿石',mercury:'水银',sulfur:'硫磺',crystal:'水晶',gem:'宝石'};
export function rng(seed){return()=>{seed|=0;seed=seed+0x6D2B79F5|0;let t=Math.imul(seed^seed>>>15,1|seed);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296;};}
const key=(x,y)=>`${x},${y}`;
export function newGame(seed=1999){const random=rng(seed),tiles=[];
 for(let y=0;y<HEIGHT;y++)for(let x=0;x<WIDTH;x++){
 const sea=(x>25&&y>16)||(x>31&&y>8)||(x>28&&y>13)||(y>24&&x>18);
 let terrain=sea?'water':x>23&&y<13?'dirt':x<8&&y>17?'sand':'grass';
 let decor=random()<.27?'tree':random()<.12?'mountain':null;
 if(terrain==='water')decor=null;
 tiles.push({x,y,terrain,decor,variant:random(),seen:false});}
 const at=(x,y)=>tiles[y*WIDTH+x];
 // A continuous road connects both castles; branch roads connect nearby sites.
 for(let x=3;x<=29;x++){let y=Math.round(20-(x-3)*.5);for(let d=-1;d<=1;d++){let t=at(x,y+d);t.terrain='grass';t.decor=null;}at(x,y).road=true;}
 for(let y=13;y<=20;y++){at(5,y).decor=null;at(5,y).road=true;}
 const objects=[{type:'town',x:4,y:20,owner:'blue',name:'狮鹫之巢'},{type:'town',x:29,y:7,owner:'red',name:'黑鸦堡',army:[{unit:'swordsman',count:12},{unit:'archer',count:18},{unit:'griffin',count:6}]},
 {type:'mine',x:8,y:18,resource:'wood',name:'伐木场',owner:null},{type:'mine',x:4,y:14,resource:'ore',name:'矿石采掘场',owner:null},
 {type:'mine',x:17,y:12,resource:'gold',name:'金矿',owner:null},{type:'mine',x:22,y:10,resource:'crystal',name:'水晶矿',owner:null},
 {type:'treasure',x:7,y:21,name:'宝箱',amount:1500},{type:'treasure',x:10,y:15,name:'宝箱',amount:1200},
 {type:'treasure',x:16,y:17,name:'宝箱',amount:2000},{type:'treasure',x:24,y:8,name:'宝箱',amount:2500},
 {type:'resource',x:6,y:17,resource:'wood',amount:8,name:'散落的木材'},{type:'resource',x:11,y:20,resource:'ore',amount:8,name:'散落的矿石'},
 {type:'resource',x:19,y:10,resource:'gem',amount:5,name:'宝石'},{type:'resource',x:15,y:14,resource:'crystal',amount:5,name:'水晶'},
 {type:'monster',x:12,y:16,name:'骷髅巡逻队',army:[{unit:'skeleton',count:30}]},{type:'monster',x:19,y:12,name:'恶狼群',army:[{unit:'wolf',count:20}]},
 {type:'monster',x:24,y:10,name:'半兽人哨兵',army:[{unit:'orc',count:20},{unit:'wolf',count:12}]},
 {type:'shrine',x:9,y:12,name:'智慧神殿',visited:false},{type:'well',x:14,y:19,name:'魔法泉'},
 {type:'artifact',x:21,y:16,name:'骑士之剑',attack:2},{type:'obelisk',x:3,y:8,name:'先知方尖碑'}];
 for(const o of objects){const t=at(o.x,o.y);t.terrain=t.terrain==='water'?'grass':t.terrain;t.decor=null;for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){const n=at(o.x+dx,o.y+dy);if(n&&n.terrain!=='water')n.decor=null;}}
 const s={version:1,seed,day:1,resources:{gold:10000,wood:20,ore:20,mercury:5,sulfur:5,crystal:5,gem:5},hero:{x:5,y:20,level:1,xp:0,attack:2,defense:2,power:1,knowledge:2,mana:20,maxMana:20,movement:1600,maxMovement:1600,army:[{unit:'pikeman',count:28},{unit:'archer',count:16},{unit:'griffin',count:4}],spells:['bolt'],artifacts:[]},town:{buildings:[],builtDay:0,pool:{pikeman:14,archer:9},income:1000},tiles,objects,log:['凯瑟琳率领军队抵达边境。占领东北方的黑鸦堡，统一这片土地。'],result:null,battle:null};reveal(s);return s;}
export function tile(s,x,y){return x>=0&&y>=0&&x<WIDTH&&y<HEIGHT?s.tiles[y*WIDTH+x]:null;}
export function objectAt(s,x,y){return s.objects.find(o=>o.x===x&&o.y===y);}
export function reveal(s){for(const t of s.tiles)if(Math.hypot(t.x-s.hero.x,t.y-s.hero.y)<6)t.seen=true;}
export function passable(s,x,y){const t=tile(s,x,y);return t&&t.terrain!=='water'&&t.decor!=='mountain'&&t.decor!=='tree';}
export function moveCost(s,x,y,dx=0,dy=0){const t=tile(s,x,y);return Math.round((t?.road?65:t?.terrain==='sand'?150:100)*(dx&&dy?Math.SQRT2:1));}
export function findPath(s,tx,ty){if(!passable(s,tx,ty)||!tile(s,tx,ty).seen)return [];
 const start=key(s.hero.x,s.hero.y),goal=key(tx,ty),open=[{x:s.hero.x,y:s.hero.y,d:0}],dist=new Map([[start,0]]),prev=new Map();
 while(open.length){open.sort((a,b)=>a.d-b.d);const cur=open.shift(),ck=key(cur.x,cur.y);if(cur.d!==dist.get(ck))continue;if(ck===goal){const out=[];let k=goal;while(k!==start){const [x,y]=k.split(',').map(Number);out.unshift({x,y,cost:dist.get(k)-dist.get(prev.get(k))});k=prev.get(k);}return out;}
 if(ck!==start){const o=objectAt(s,cur.x,cur.y);if(o&&(o.type==='monster'||(o.type==='town'&&o.owner==='red')))continue;}
 for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){if(!dx&&!dy)continue;const x=cur.x+dx,y=cur.y+dy;if(!passable(s,x,y)||!tile(s,x,y).seen)continue;if(dx&&dy&&(!passable(s,cur.x+dx,cur.y)||!passable(s,cur.x,cur.y+dy)))continue;const d=cur.d+moveCost(s,x,y,dx,dy),k=key(x,y);if(d<(dist.get(k)??Infinity)){dist.set(k,d);prev.set(k,ck);open.push({x,y,d});}}
 }return [];}
export function addLog(s,message){s.log.push(message);if(s.log.length>50)s.log.shift();return message;}
export function moveStep(s,p){if(s.battle||s.result)return '当前无法移动';if(Math.max(Math.abs(p.x-s.hero.x),Math.abs(p.y-s.hero.y))!==1||!passable(s,p.x,p.y))return '无法通行';const cost=moveCost(s,p.x,p.y,p.x-s.hero.x,p.y-s.hero.y);if(s.hero.movement<cost)return '行动力不足，请结束回合';s.hero.movement-=cost;s.hero.x=p.x;s.hero.y=p.y;reveal(s);const o=objectAt(s,p.x,p.y);if(!o)return null;return interact(s,o);}
export function interact(s,o){let msg='';switch(o.type){case 'treasure':s.resources.gold+=o.amount;msg=`发现宝箱，获得 ${o.amount} 金币。`;break;case 'resource':s.resources[o.resource]+=o.amount;msg=`获得 ${o.amount} ${RESOURCE_NAMES[o.resource]}。`;break;case 'artifact':s.hero.attack+=o.attack;s.hero.artifacts.push(o.name);msg=`获得${o.name}，攻击 +${o.attack}。`;break;case 'mine':if(o.owner!=='blue'){o.owner='blue';msg=`占领${o.name}，每天获得 ${o.resource==='gold'?1000:2} ${RESOURCE_NAMES[o.resource]}。`;}else msg=`${o.name}正在为你生产资源。`;break;case 'monster':startBattle(s,o);return '遭遇敌军，战斗开始！';case 'town':if(o.owner==='red'){startBattle(s,o);return '黑鸦堡守军迎战！';}msg='进入狮鹫之巢，可建设城镇与招募军队。';break;case 'shrine':if(!o.visited){o.visited=true;gainXP(s,600);msg='智慧神殿传授了古老的知识，经验 +600。';}else msg='你已拜访过智慧神殿。';break;case 'well':s.hero.mana=s.hero.maxMana;msg='魔法泉恢复了全部魔法值。';break;case 'obelisk':for(const t of s.tiles)if(Math.hypot(t.x-29,t.y-7)<5)t.seen=true;msg='先知揭示了黑鸦堡的位置：地图东北方。';break;}
 if(['treasure','resource','artifact'].includes(o.type))s.objects=s.objects.filter(v=>v!==o);return addLog(s,msg);}
export function gainXP(s,amount){s.hero.xp+=amount;while(s.hero.xp>=s.hero.level*1000){s.hero.xp-=s.hero.level*1000;s.hero.level++;s.hero.attack++;s.hero.defense++;s.hero.maxMana+=10;s.hero.knowledge++;addLog(s,`凯瑟琳升至 ${s.hero.level} 级，攻击与防御 +1，知识 +1。`);}}
export function canAfford(s,cost){return Object.entries(cost).every(([k,n])=>s.resources[k]>=n);}
export function build(s,id){const b=BUILDINGS[id];if(!b)return '无效建筑';if(s.town.buildings.includes(id))return '已经建造';if(s.town.builtDay===s.day)return '每个城镇每天只能建造一次';if(!b.requires.every(r=>s.town.buildings.includes(r)))return '尚未满足前置建筑要求';if(!canAfford(s,b.cost))return '资源不足';for(const [k,n] of Object.entries(b.cost))s.resources[k]-=n;s.town.buildings.push(id);s.town.builtDay=s.day;if(b.unit)s.town.pool[b.unit]=UNITS[b.unit].growth;if(id==='hall')s.town.income+=1000;if(id==='mage'){s.hero.spells=['bolt','heal'];s.hero.mana=s.hero.maxMana;}return addLog(s,`${b.name}建造完成。`);}
export function recruit(s,unit,number){const u=UNITS[unit];number=Math.floor(number);if(!u||!Number.isFinite(number)||number<=0||number>(s.town.pool[unit]||0))return '可招募数量不足';if(Math.hypot(s.hero.x-4,s.hero.y-20)>2)return '英雄需要返回狮鹫之巢附近才能招募';if(s.resources.gold<number*u.cost)return '金币不足';let stack=s.hero.army.find(a=>a.unit===unit);if(!stack&&s.hero.army.length>=7)return '部队最多容纳七种兵种';s.resources.gold-=number*u.cost;s.town.pool[unit]-=number;if(stack)stack.count+=number;else s.hero.army.push({unit,count:number});return addLog(s,`招募了 ${number} 名${u.name}。`);}
export function endDay(s){if(s.battle||s.result)return '当前无法结束回合';s.day++;s.resources.gold+=s.town.income;for(const o of s.objects.filter(o=>o.type==='mine'&&o.owner==='blue'))s.resources[o.resource]+=o.resource==='gold'?1000:2;s.hero.movement=s.hero.maxMovement;s.hero.mana=Math.min(s.hero.maxMana,s.hero.mana+1);if(s.town.buildings.includes('mage'))s.hero.mana=s.hero.maxMana;if((s.day-1)%7===0){for(const u of Object.keys(s.town.pool))s.town.pool[u]+=UNITS[u].growth;addLog(s,'新的一周开始了，城镇生物数量增加。');}
 if(s.day%7===0){const enemy=s.objects.find(o=>o.owner==='red'&&o.type==='town');if(enemy){enemy.army[0].count+=2;enemy.army[1].count+=3;}}
 return addLog(s,`第 ${s.day} 天：获得城镇收入 ${s.town.income} 金币，行动力已恢复。`);}
function battleStack(a,team,i){const u=UNITS[a.unit];return{...a,team,id:team+i,x:team==='blue'?1:13,y:[0,2,3,5,7,8,10][i],initialCount:a.count,hp:a.count*u.hp,retaliated:false,defending:false,waited:false,shots:12};}
export function startBattle(s,o){const units=[...s.hero.army.map((a,i)=>battleStack(a,'blue',i)),...o.army.map((a,i)=>battleStack(a,'red',i))];s.battle={target:{x:o.x,y:o.y},round:1,units,queue:[],log:[`与${o.name}的战斗开始！`],spellRound:0};newRound(s);}
function newRound(s){const b=s.battle;b.queue=b.units.filter(u=>u.hp>0).sort((a,c)=>UNITS[c.unit].speed-UNITS[a.unit].speed||a.team.localeCompare(c.team)).map(u=>u.id);for(const u of b.units){u.retaliated=false;u.defending=false;u.waited=false;}}
export function currentUnit(s){const b=s.battle;return b?.units.find(u=>u.id===b.queue[0]&&u.hp>0);}
export function hexDistance(a,b){const cube=p=>{const q=p.x-(p.y-(p.y&1))/2;return[q,-q-p.y,p.y];};const ac=cube(a),bc=cube(b);return Math.max(...ac.map((n,i)=>Math.abs(n-bc[i])));}
export function battlePath(b,u,x,y){const occupied=(x,y)=>b.units.some(a=>a.hp>0&&a.id!==u.id&&a.x===x&&a.y===y);if(x<0||x>=BATTLE_WIDTH||y<0||y>=BATTLE_HEIGHT||occupied(x,y))return null;if(UNITS[u.unit].flying)return hexDistance(u,{x,y});const queue=[{x:u.x,y:u.y,d:0}],seen=new Set([key(u.x,u.y)]);while(queue.length){const p=queue.shift();if(p.x===x&&p.y===y)return p.d;for(const [dx,dy] of [[-1,0],[1,0],[p.y%2?0:-1,-1],[p.y%2?1:0,-1],[p.y%2?0:-1,1],[p.y%2?1:0,1]]){const nx=p.x+dx,ny=p.y+dy,k=key(nx,ny);if(nx>=0&&nx<BATTLE_WIDTH&&ny>=0&&ny<BATTLE_HEIGHT&&!seen.has(k)&&!occupied(nx,ny)){seen.add(k);queue.push({x:nx,y:ny,d:p.d+1});}}}return null;}
function hurt(b,target,damage){const old=target.count;target.hp=Math.max(0,target.hp-damage);target.count=Math.ceil(target.hp/UNITS[target.unit].hp);b.log.push(`${UNITS[target.unit].name}受到 ${damage} 点伤害，损失 ${old-target.count} 个单位。`);}
function strike(s,a,t,retaliation=false){const b=s.battle,u=UNITS[a.unit],v=UNITS[t.unit],attack=u.attack+(a.team==='blue'?s.hero.attack:2),defense=v.defense+(t.team==='blue'?s.hero.defense:2)+(t.defending?Math.ceil(v.defense*.2):0);const delta=attack-defense,factor=delta>=0?1+Math.min(delta,60)*.05:Math.max(.3,1+delta*.025);let damage=Math.max(1,Math.floor(a.count*(u.min+u.max)/2*factor));const ranged=u.ranged&&hexDistance(a,t)>1;if(ranged){a.shots--;if(hexDistance(a,t)>6)damage=Math.max(1,Math.floor(damage/2));}b.log.push(`${UNITS[a.unit].name}${retaliation?'反击':'攻击'}${UNITS[t.unit].name}。`);hurt(b,t,damage);if(t.hp>0&&!ranged&&!retaliation&&(!t.retaliated||t.unit==='griffin')){t.retaliated=true;strike(s,t,a,true);}}
function checkBattle(s){const b=s.battle,blue=b.units.some(u=>u.team==='blue'&&u.hp>0),red=b.units.some(u=>u.team==='red'&&u.hp>0);if(blue&&red)return false;const target=objectAt(s,b.target.x,b.target.y);s.hero.army=b.units.filter(u=>u.team==='blue'&&u.hp>0).map(({unit,count})=>({unit,count}));if(!blue){s.result='defeat';addLog(s,'凯瑟琳的军队全军覆没。战役失败。');}else{gainXP(s,500+Math.round(b.units.filter(u=>u.team==='red').reduce((n,u)=>n+UNITS[u.unit].hp*u.initialCount,0)));s.resources.gold+=500;if(target?.type==='town'){target.owner='blue';s.result='victory';addLog(s,'黑鸦堡已被占领！边境重归和平。');}else{s.objects=s.objects.filter(o=>o!==target);addLog(s,'战斗胜利！获得 500 金币和战斗经验。');}}s.battle=null;return true;}
function finishAction(s){const b=s.battle;if(checkBattle(s))return;b.queue=b.queue.slice(1).filter(id=>b.units.some(u=>u.id===id&&u.hp>0));if(!b.queue.length){b.round++;newRound(s);}if(b.log.length>80)b.log=b.log.slice(-60);}
export function battleAction(s,action,x,y){const b=s.battle,u=currentUnit(s);if(!b||!u)return '没有正在进行的战斗';if(action==='defend'){u.defending=true;b.log.push(`${UNITS[u.unit].name}进入防御姿态。`);finishAction(s);return null;}if(action==='wait'){if(u.waited)return '本回合已等待过';u.waited=true;b.queue.push(b.queue.shift());b.log.push(`${UNITS[u.unit].name}等待。`);return null;}if(action==='move'){const d=battlePath(b,u,x,y);if(d===null||d>UNITS[u.unit].speed)return '无法到达该位置';u.x=x;u.y=y;b.log.push(`${UNITS[u.unit].name}移动到新的位置。`);finishAction(s);return null;}
 if(action==='attack'){const target=b.units.find(t=>t.x===x&&t.y===y&&t.hp>0&&t.team!==u.team);if(!target)return '请选择敌方单位';const ranged=UNITS[u.unit].ranged&&u.shots>0&&!b.units.some(t=>t.hp>0&&t.team!==u.team&&hexDistance(u,t)===1);if(hexDistance(u,target)>1&&!ranged){const options=[];for(let yy=0;yy<BATTLE_HEIGHT;yy++)for(let xx=0;xx<BATTLE_WIDTH;xx++){if(hexDistance({x:xx,y:yy},target)!==1)continue;const d=battlePath(b,u,xx,yy);if(d!==null&&d<=UNITS[u.unit].speed)options.push({x:xx,y:yy,d});}options.sort((a,b)=>a.d-b.d);if(!options.length)return '敌军不在移动攻击范围内';u.x=options[0].x;u.y=options[0].y;}strike(s,u,target);finishAction(s);return null;}return '未知指令';}
export function castSpell(s,spell,x,y){const b=s.battle,u=currentUnit(s);if(!b||u?.team!=='blue')return '等待我方单位行动';if(b.spellRound===b.round)return '英雄每轮只能施法一次';if(!s.hero.spells.includes(spell))return '尚未学会该魔法';const cost=spell==='bolt'?5:6;if(s.hero.mana<cost)return '魔法值不足';const target=b.units.find(t=>t.x===x&&t.y===y&&t.hp>0&&t.team===(spell==='bolt'?'red':'blue'));if(!target)return spell==='bolt'?'请选择敌方目标':'请选择我方目标';s.hero.mana-=cost;b.spellRound=b.round;if(spell==='bolt'){hurt(b,target,10*s.hero.power+20);}else{const heal=Math.min(target.count*UNITS[target.unit].hp-target.hp,30+10*s.hero.power);target.hp+=heal;b.log.push(`治疗术恢复了 ${heal} 点生命，无法复活已阵亡单位。`);}checkBattle(s);return null;}
export function aiAction(s){const b=s.battle,u=currentUnit(s);if(!u||u.team!=='red')return;const targets=b.units.filter(t=>t.team==='blue'&&t.hp>0).sort((a,c)=>hexDistance(u,a)-hexDistance(u,c));if(!targets.length)return;const target=targets[0];if(!battleAction(s,'attack',target.x,target.y))return;const options=[];for(let y=0;y<BATTLE_HEIGHT;y++)for(let x=0;x<BATTLE_WIDTH;x++){const d=battlePath(b,u,x,y);if(d!==null&&d>0&&d<=UNITS[u.unit].speed)options.push({x,y,score:hexDistance({x,y},target)});}options.sort((a,c)=>a.score-c.score);if(options.length)battleAction(s,'move',options[0].x,options[0].y);else battleAction(s,'defend');}
export function validateSave(s){if(!s||s.version!==1||s.tiles?.length!==WIDTH*HEIGHT||!Array.isArray(s.objects)||!s.hero||!Array.isArray(s.hero.army)||!s.town||!Array.isArray(s.town.buildings)||!s.town.pool||!Array.isArray(s.log))return false;if(!Number.isInteger(s.day)||s.day<1||!Number.isInteger(s.hero.x)||!Number.isInteger(s.hero.y)||!tile(s,s.hero.x,s.hero.y))return false;if(!Object.keys(RESOURCE_NAMES).every(k=>Number.isFinite(s.resources?.[k])&&s.resources[k]>=0))return false;return s.hero.army.every(a=>UNITS[a.unit]&&Number.isInteger(a.count)&&a.count>0)&&s.tiles.every(t=>['grass','sand','dirt','water'].includes(t.terrain));}
