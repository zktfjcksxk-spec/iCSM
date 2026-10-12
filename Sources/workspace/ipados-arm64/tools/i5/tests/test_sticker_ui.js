// Run real inventory JS with a small Panorama/API harness, including nested navigation.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const panels={};let state,saves=[],imageCalls=[],failSave=false,skipWrite=false;const recipes={};
class Panel {
 constructor(id){this.id=id;this.style={};this.enabled=true;this.children=[];this.events={};panels[id]=this;}
 FindChildInLayoutFile(id){return panels[id]||new Panel(id);}
 FindChildTraverse(id){return panels[id];}
 SetHasClass(){} AddClass(){} SetImage(value){imageCalls.push(value);} RemoveAndDeleteChildren(){this.children=[];}
 SetPanelEvent(name,fn){this.events[name]=fn;}
 SetAttributeString(name,value){if(name==='icsm-inventory-state')state=JSON.parse(value);}
}
const root=new Panel('root');
const items=[{definition:36,kind:'secondary',name:'P250'}, {definition:61,kind:'secondary',name:'USP'}, {definition:4,kind:'secondary',name:'Glock'}, {definition:515,kind:'melee',name:'Butterfly'}];
const context={console,$:{Schedule(){},GetContextPanel:()=>root,Localize:s=>'localized '+s,CreatePanel:(type,parent,id)=>{const p=new Panel(id);parent.children.push(p);return p;},RegisterForUnhandledEvent(){},DispatchEvent:(event,p)=>p.events.onactivate(),Msg(){}},NativeInventoryAPI:{
 GetInventory:team=>items.filter(i=>!(i.definition===61&&team===2)&&!(i.definition===4&&team===3)).map(i=>({...i,slot:2,paint:0,image:'file://icsm-sticker-previews/preview_36_0_1_0_0_0_0_39.png',skin_name:'original',equipped:false})),
 GetSkins:(team,def)=>[0,10,20].map(p=>({paint:p,name:'skin '+p,image:def+'_'+p+'.png',selected:p===0,sticker_slots:def===515?0:4})),
 GetStickerCatalog:()=>[0,1,4000].map(id=>({sticker:id,name:'sticker '+id,image:'sticker_'+id+'.png'})),
 GetStickerSlots:(team,def,paint)=>[0,1,2,3].map(slot=>({slot,sticker:recipes[[def,paint,slot]]||0,name:'slot'})),
 GetPreviewRevision:()=>{throw new Error('Sticker preview polling must be disabled');},
 GetPreview:()=>{throw new Error('Sticker image generation must be disabled');},
 SaveSticker:(team,def,paint,slot,id)=>{saves.push({team,def,paint,slot,id});if(failSave)return false;if(!skipWrite)recipes[[def,paint,slot]]=id;return true;},
 SaveSkin:()=>true,EquipSkin:()=>true,Equip:()=>true}};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),context);
assert(state.items.every(row=>/^file:\/\/\{images\}\/offline_skins\/[0-9]+_[0-9]+\.png$/.test(row.image))); // stale sticker cache is never shown
assert(imageCalls.length>0 && imageCalls.every(url=>!url || url.indexOf('file://{images}/')===0)); // backing never resolves the crashing relative URI
const activate=id=>context.OfflineInventory.ValidateUI('activate',id);
activate('OfflineItem36');assert.equal(state.mode,'skins');assert(state.items.every(i=>i.sticker_action));
activate('OfflineSkin20');activate('OfflineStickerOpen10');assert.equal(state.mode,'stickers');assert.equal(state.stickers.paint,10);
activate('OfflineStickerSlot2');activate('OfflineStickerPick4000');assert.equal(state.stickers.slot,2);assert.equal(state.stickers.pending,4000);
context.OfflineInventory.Confirm();assert.deepEqual(saves[0],{team:3,def:36,paint:10,slot:2,id:4000});
assert.equal(state.skin.image,'file://{images}/offline_skins/36_10.png'); // stickers never change the skin artwork
// Replace a filled slot repeatedly in one visit, without removing it first.
activate('OfflineStickerPick1');assert.equal(state.skin.confirm_text,'确认替换');context.OfflineInventory.Confirm();
assert.equal(state.stickers.slots[2].sticker,1);assert(state.status.includes('已替换贴纸'));
activate('OfflineStickerPick4000');context.OfflineInventory.Confirm();assert.equal(state.stickers.slots[2].sticker,4000);
activate('OfflineStickerSlot1');activate('OfflineStickerPick1');context.OfflineInventory.Confirm();
activate('OfflineStickerSlot2');activate('OfflineStickerPick1');context.OfflineInventory.Confirm();
assert.equal(state.stickers.slots[1].sticker,1);assert.equal(state.stickers.slots[2].sticker,1);
assert(saves.every(s=>s.id!==0)); // replacement has no intermediate empty-slot save
// Failed writes and false-success responses must not be announced as a replacement.
failSave=true;activate('OfflineStickerPick4000');context.OfflineInventory.Confirm();assert(state.status.includes('失败'));
assert.equal(state.stickers.slots[2].sticker,1);failSave=false;
skipWrite=true;context.OfflineInventory.Confirm();assert(state.status.includes('失败'));skipWrite=false;
context.OfflineInventory.Confirm();assert.equal(state.stickers.slots[2].sticker,4000);
context.OfflineInventory.Back();assert.equal(state.mode,'skins');assert.equal(state.skin.name,'skin 20'); // no implicit skin change
assert.equal(state.items.find(i=>i.paint===10).image,'file://{images}/offline_skins/36_10.png');
assert.equal(state.skin.image,'file://{images}/offline_skins/36_20.png');
activate('OfflineStickerOpen20');assert.equal(state.stickers.pending,0); // per skin, not per gun
activate('OfflineStickerSlot99');activate('OfflineStickerPick99999');assert.equal(state.stickers.slot,0);assert.equal(state.stickers.pending,0);
context.OfflineInventory.Back();activate('OfflineStickerOpen10');activate('OfflineStickerSlot2');assert.equal(state.stickers.pending,4000);
activate('OfflineStickerPick0');assert.equal(state.skin.confirm_text,'移除贴纸');context.OfflineInventory.Confirm();assert.equal(saves.at(-1).id,0);
assert.equal(state.stickers.slots[1].sticker,1);assert.equal(state.stickers.slots[2].sticker,0);
context.OfflineInventory.Back();context.OfflineInventory.Back();activate('OfflineItem515');assert(state.items.every(i=>!i.sticker_action));
assert(imageCalls.every(url=>!url || /^file:\/\/\{images\}\/offline_skins\/[0-9]+_[0-9]+\.png$/.test(url)));
console.log('PASS repeated occupied-slot replacements without removal, other-slot preservation, save failure/readback handling, static skin artwork, nested navigation, exact skin persistence, removal, invalid actions, knives excluded');
// Gloves use the same per-team list and separate skin/equip actions as knives.
const api=context.NativeInventoryAPI,oldInventory=api.GetInventory,oldSkins=api.GetSkins;
const gloveRecipes={3:10018,2:10018},equippedGloves={3:5029,2:5028};
const skinSaves=[],skinEquips=[],equipment=[];let failSkinSave=false;
api.GetInventory=team=>oldInventory(team).concat([team===3?5029:5028,5030].map(def=>({
 definition:def,kind:'gloves',name:'glove '+def,slot:41,
 paint:def===5030?gloveRecipes[team]:0,skin_name:'glove finish',
 image:'file://{images}/offline_skins/'+def+'_'+(def===5030?gloveRecipes[team]:0)+'.png',equipped:equippedGloves[team]===def})));
api.GetSkins=(team,def)=>def<5028?oldSkins(team,def):(def===5030?[10018,10037]:[0]).map(p=>({
 paint:p,name:'glove '+p,image:'file://{images}/offline_skins/'+def+'_'+p+'.png',
 selected:p===(def===5030?gloveRecipes[team]:0),sticker_slots:0}));
api.SaveSkin=(team,def,paint)=>{skinSaves.push({team,def,paint});if(failSkinSave)return false;if(def===5030)gloveRecipes[team]=paint;return true;};
api.EquipSkin=(team,def,paint)=>{skinEquips.push({team,def,paint});if(def<5028)return true;equippedGloves[team]=def;if(def===5030)gloveRecipes[team]=paint;return true;};
api.Equip=(team,slot,def)=>{equipment.push({team,slot,def});if(def>=5028)equippedGloves[team]=def;return true;};
context.OfflineInventory.Back();context.OfflineInventory.SetCategory('gloves');
assert.equal(state.categories.length,9);assert.equal(state.mode,'gloves');assert.equal(state.items.length,4);
assert.deepEqual(state.items.map(i=>[i.definition,i.side]),[[5029,3],[5028,2],[5030,3],[5030,2]]);
assert(state.items.every(i=>i.kind==='gloves'));assert.equal(new Set(state.items.map(i=>i.id)).size,4);
activate('OfflineGloveCT5030');assert.equal(state.mode,'skins');assert(!state.skin.shared);assert(state.skin.ct&&state.skin.t);
assert(state.skin.glove&&state.skin.save_only&&state.skin.locked_team);assert.equal(state.skin.team,3);
context.OfflineInventory.SetSkinTeam(2);assert.equal(state.skin.team,3); // a column cannot silently save to the other team
assert.equal(state.items.length,2);assert(state.items.every(i=>!i.sticker_action));
activate('OfflineSkin10037');context.OfflineInventory.Confirm();
assert.deepEqual(skinSaves.at(-1),{team:3,def:5030,paint:10037});assert.equal(skinEquips.length,0);
assert.equal(equippedGloves[3],5029);assert.equal(gloveRecipes[3],10037);assert.equal(equippedGloves[2],5028);assert.equal(gloveRecipes[2],10018);
assert.equal(state.mode,'gloves');
assert.equal(state.items.find(i=>i.id==='OfflineGloveCT5030').image,'file://{images}/offline_skins/5030_10037.png');
assert.equal(state.items.find(i=>i.id==='OfflineGloveT5030').image,'file://{images}/offline_skins/5030_10018.png');
activate('OfflineEquipCT5030');assert.equal(equippedGloves[3],5030);assert.equal(equippedGloves[2],5028);
assert.deepEqual(equipment.at(-1),{team:3,slot:41,def:5030});
activate('OfflineGloveT5030');assert.equal(state.skin.team,2);context.OfflineInventory.SetSkinTeam(3);assert.equal(state.skin.team,2);
activate('OfflineSkin10037');failSkinSave=true;context.OfflineInventory.Confirm();
assert.equal(state.mode,'skins');assert(state.skin.confirm);assert(state.status.includes('失败'));
assert.equal(gloveRecipes[2],10018);assert.equal(equippedGloves[2],5028);failSkinSave=false;
context.OfflineInventory.Confirm();assert.equal(state.mode,'gloves');assert.equal(gloveRecipes[2],10037);assert.equal(equippedGloves[2],5028);
assert.equal(gloveRecipes[3],10037);activate('OfflineEquipT5030');assert.equal(equippedGloves[2],5030);
// Both same-model columns must keep their own saved recipe and thumbnail.
activate('OfflineGloveT5030');activate('OfflineSkin10018');context.OfflineInventory.Confirm();
assert.equal(gloveRecipes[2],10018);assert.equal(gloveRecipes[3],10037);
activate('OfflineGloveCT5029');assert(state.skin.ct&&!state.skin.t);activate('OfflineSkin0');context.OfflineInventory.Confirm();
assert.equal(equippedGloves[3],5030);activate('OfflineEquipCT5029');
assert.equal(equippedGloves[3],5029);assert.equal(equippedGloves[2],5030);assert.equal(gloveRecipes[3],10037);
const beforeInvalid=equipment.length;activate('OfflineEquipT5029');assert.equal(equipment.length,beforeInvalid);
// The all-items view still permits choosing a side, without auto-equipping.
context.OfflineInventory.SetCategory('all');activate('OfflineItem5030');assert(!state.skin.locked_team);
context.OfflineInventory.SetSkinTeam(3);assert.equal(state.skin.team,3);activate('OfflineSkin10018');context.OfflineInventory.Confirm();
assert.equal(gloveRecipes[3],10018);assert.equal(equippedGloves[3],5029);assert.equal(skinEquips.length,0);
context.OfflineInventory.SetCategory('melee');assert.equal(state.mode,'knives');
assert.deepEqual(state.items.map(i=>[i.id,i.side]),[['OfflineKnifeCT515',3],['OfflineKnifeT515',2]]);
activate('OfflineKnifeT515');assert(state.skin.knife&&state.skin.save_only&&state.skin.locked_team);
assert.equal(state.skin.team,2);context.OfflineInventory.SetSkinTeam(3);assert.equal(state.skin.team,2);
activate('OfflineSkin20');context.OfflineInventory.Confirm();assert.deepEqual(skinSaves.at(-1),{team:2,def:515,paint:20});
assert.equal(state.mode,'knives');assert.equal(skinEquips.length,0);
console.log('PASS glove CT/T lists, unique team actions, column lock, independent recipes and thumbnails, save-only confirmation, explicit equip, failed save retry, default restoration, invalid-team rejection, all-items navigation and unchanged knife behavior');
// Enumerate the generated production catalog through the actual page events.
const path=require('path');
const catalogSource=fs.readFileSync(path.resolve(__dirname,'../../../inventory/offline_glove_catalog.h'),'utf8');
const fullGloves=[...catalogSource.matchAll(/\{(\d+),(\d+),("(?:\\.|[^"\\])*")\}/g)].map(m=>({definition:+m[1],paint:+m[2],name:JSON.parse(m[3])}));
assert.equal(fullGloves.length,48);
const families=[...new Set(fullGloves.map(r=>r.definition))];assert.equal(families.length,7);
const fullRecipes={},fullEquipped={3:5029,2:5028};
const savedPaint=(team,def)=>fullRecipes[team+'-'+def]||fullGloves.find(r=>r.definition===def)?.paint||0;
api.GetInventory=team=>oldInventory(team).concat([team===3?5029:5028,...families].map(def=>({
 definition:def,kind:'gloves',name:'glove '+def,slot:41,paint:savedPaint(team,def),
 image:'file://{images}/offline_skins/'+def+'_'+savedPaint(team,def)+'.png',skin_name:'finish',equipped:fullEquipped[team]===def})));
api.GetSkins=(team,def)=>def<5027?oldSkins(team,def):(fullGloves.filter(r=>r.definition===def).length?fullGloves.filter(r=>r.definition===def):[{definition:def,paint:0,name:'default'}]).map(r=>({
 paint:r.paint,name:r.name,image:'file://{images}/offline_skins/'+def+'_'+r.paint+'.png',selected:r.paint===savedPaint(team,def),sticker_slots:0}));
api.SaveSkin=(team,def,paint)=>{
 if(!fullGloves.some(r=>r.definition===def&&r.paint===paint) && !(def===(team===3?5029:5028)&&paint===0))return false;
 fullRecipes[team+'-'+def]=paint;return true;
};
api.Equip=(team,slot,def)=>{assert.equal(slot,41);assert(families.includes(def)||def===(team===3?5029:5028));fullEquipped[team]=def;return true;};
api.EquipSkin=()=>{throw new Error('Glove selection must save without auto-equipping');};
context.OfflineInventory.SetCategory('gloves');assert.equal(state.mode,'gloves');assert.equal(state.title,'');
assert.equal(state.items.length,16);assert.equal(new Set(state.items.map(r=>r.id)).size,16);
state.items.forEach((r,i)=>assert.equal(r.side,i%2===0?3:2));
for(const team of [3,2])for(const family of families) {
 const side=team===3?'CT':'T';activate('OfflineGlove'+side+family);
 assert.equal(state.skin.team,team);assert(state.skin.locked_team&&state.skin.save_only);
 const choices=fullGloves.filter(r=>r.definition===family);
 assert.equal(state.items.length,choices.length);assert(state.items.every(r=>!r.sticker_action));
 for(const recipe of choices) {
  activate('OfflineSkin'+recipe.paint);context.OfflineInventory.Confirm();
  assert.equal(state.mode,'gloves');assert.equal(fullEquipped[team],team===3?5029:5028);
  const row=state.items.find(r=>r.id==='OfflineGlove'+side+family);
  assert.equal(row.paint,recipe.paint);assert.equal(row.image,'file://{images}/offline_skins/'+family+'_'+recipe.paint+'.png');
  activate(row.id);
 }
 context.OfflineInventory.Back();
}
for(const family of families) {
 activate('OfflineEquipCT'+family);assert.equal(fullEquipped[3],family);assert.equal(fullEquipped[2],5028);
}
activate('OfflineEquipT5035');activate('OfflineEquipCT5029');
assert.equal(fullEquipped[3],5029);assert.equal(fullEquipped[2],5035);
console.log('PASS production glove catalog: 16 aligned CT/T rows, all 48 skins selectable/saved independently for both teams, all seven families equip, native artwork paths, default restoration and title removal');
