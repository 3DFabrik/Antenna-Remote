const char MAIN_page[] PROGMEM = R"=====(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Antenna Remote</title>
<style>
*{box-sizing:border-box}
[hidden]{display:none!important}
html{font-size:clamp(9px,.9vw + .9vh,34px)}
html,body{height:100%}
body{margin:0;background:#1c1c1c;color:#eee;font-family:system-ui,"Segoe UI",Arial,sans-serif;display:flex;flex-direction:column;min-width:0}
header{display:flex;align-items:center;flex-wrap:wrap;gap:.3rem .3rem;padding:.35rem .6rem;background:#333;flex:none}
h1{font-size:1rem;margin:0 .6rem 0 0;white-space:nowrap}
.sp{flex:1}
.tab{background:#444;color:#eee;border:0;padding:.25rem .6rem;border-radius:.3rem;cursor:pointer;font-size:.85rem}
.tab.on{background:#6b8e23}
#hver{font-size:.75rem;color:#aaa;white-space:nowrap;margin-left:.4rem}
#link{display:flex;align-items:center;gap:.35rem;font-size:.75rem;color:#aaa;white-space:nowrap}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:.9rem}
.bars i{display:block;width:.28rem;background:#444;border-radius:1px}
.bars i:nth-child(1){height:35%}
.bars i:nth-child(2){height:65%}
.bars i:nth-child(3){height:100%}
.bars i.on{background:#00e5ff}
#sdr{display:inline-flex;align-items:center;gap:.25rem;color:#666}
#sdr::before{content:"";width:.55rem;height:.55rem;border-radius:50%;background:#444}
#sdr.on{color:#7dff8a}
#sdr.on::before{background:#7dff8a;box-shadow:0 0 .35rem #2cff4f}
#conn{font-size:.75rem;color:#9a9;white-space:nowrap}
#conn.bad{color:#e74c3c}
main{flex:1;min-height:0;min-width:0;padding:.5rem .6rem;overflow:auto}
#main{display:flex;flex-direction:column}

/* Everything on the main page takes its text size from the box it sits in (container query units). */
.top{display:flex;gap:.5rem;align-items:stretch;flex:none}
.lcd{flex:1;min-width:0;container-type:size;aspect-ratio:3/1;background:#0b1a0e;border:2px solid #333;border-radius:.4rem;box-shadow:inset 0 0 1.2rem #000;overflow:hidden}
.lcdin{display:grid;grid-template-columns:1fr auto;grid-template-rows:1fr auto;height:100%;padding:4cqh 4cqw;column-gap:2cqw}
.digits{position:relative;align-self:center;justify-self:end;font-family:"Courier New",Consolas,ui-monospace,monospace;font-weight:700;font-size:min(50cqh,16cqw);line-height:1;white-space:nowrap}
.digits .ghost{display:block;color:#143019;text-align:right}
#qrg{position:absolute;right:0;top:0;color:#7dff8a;text-shadow:0 0 .3em #2cff4f99;white-space:nowrap}
.unit{align-self:center;font-family:"Courier New",Consolas,ui-monospace,monospace;font-weight:700;font-size:min(16cqh,5cqw);color:#4fbf5d}
#sub{grid-column:1/3;font-family:"Courier New",Consolas,ui-monospace,monospace;font-size:max(9px,min(11cqh,3.6cqw));color:#4fbf5d;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ctl{display:flex;flex-direction:column;gap:.3rem;width:clamp(6.5rem,30vw,24rem);flex:none}
.cell{flex:1 1 0;min-height:0;container-type:size}
.cell.ghost{visibility:hidden}
.ctl .cell{min-height:1.6rem}
.cb,.ant{width:100%;height:100%;border:0;cursor:pointer;font-weight:700;overflow:hidden;padding:0 .3em;white-space:nowrap}
.cb{background:#666;color:#fff;border-radius:.35rem;font-size:min(46cqh,calc(90cqw/(var(--len,8)*.7)))}
.cb.on{background:#2e8b57}
.cb.red{background:#c0392b}
.cb:disabled{opacity:.4;cursor:default}
.ants{flex:1;min-height:0;display:flex;flex-direction:column;gap:.3rem;margin-top:.5rem}
.ants .cell{min-height:2.2rem;max-height:14rem}
.ant{background:#000;color:#888;border:2px solid #444;border-radius:.4rem;font-size:min(46cqh,calc(92cqw/(var(--len,8)*.62)))}
.ant.sel{background:#6b8e23;color:#fff;border-color:#9acd32}
.ant:disabled{cursor:default}

h2{font-size:.9rem;margin:.9rem 0 .3rem;font-style:italic}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:.1rem 1.2rem;max-width:44rem}
.row{display:flex;align-items:center;gap:.4rem;margin:.2rem 0;max-width:44rem}
.row label.l{width:4.8rem;flex:none;color:#bbb;font-size:.85rem}
input[type=text],input[type=number],select{background:#fff;color:#000;border:1px solid #888;border-radius:.25rem;padding:.2rem .35rem;font-size:.85rem}
input[type=text]{flex:1;min-width:0}
select{flex:1;min-width:0}
.ext{white-space:nowrap;color:#bbb;font-size:.8rem}
.btns{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.9rem}
.btns button{background:#555;color:#fff;border:0;border-radius:.3rem;padding:.4rem .9rem;font-size:.85rem;cursor:pointer}
.btns button.go{background:#2e8b57}
.btns button.warn{background:#a33}
.btns button:disabled{opacity:.4;cursor:default}
.note{color:#aaa;line-height:1.4;font-size:.85rem;max-width:44rem}
#msg,#umsg{margin-top:.6rem;min-height:1.3em;color:#9acd32;font-size:.85rem}
#msg.err,#umsg.err{color:#e74c3c}
input[type=file]{color:#ddd;max-width:100%;font-size:.85rem}
#toast{position:fixed;left:50%;bottom:1rem;transform:translateX(-50%);background:#2e8b57;color:#fff;padding:.5rem 1rem;border-radius:.4rem;font-size:.9rem;box-shadow:0 2px 10px #000}
@media(max-width:520px){h1{display:none}.grid2{grid-template-columns:1fr}}
@media(max-width:420px){.lcd{aspect-ratio:2.2/1}.tab{padding:.2rem .4rem}#hver,#conn{font-size:.65rem}#rssi{display:none}}
</style>
</head>
<body>
<header>
<h1>Antenna Remote</h1>
<button class="tab on" id="tabMain" onclick="tab('main')">Main</button>
<button class="tab" id="tabSet" onclick="tab('set')">Settings</button>
<button class="tab" id="tabUpd" onclick="tab('upd')">Update</button>
<span class="sp"></span>
<span id="link" title="WLAN signal and SDROxide connection"><span class="bars"><i></i><i></i><i></i></span><span id="rssi"></span><span id="sdr">SDROxide</span></span>
<span id="hver">FW -</span>
<span id="conn">connecting...</span>
</header>

<main id="main">
<div class="top">
<div class="lcd"><div class="lcdin">
<div class="digits"><span class="ghost">88.8888</span><span id="qrg">-.----</span></div>
<div class="unit">MHz</div>
<div id="sub"></div>
</div></div>
<div class="ctl">
<div class="cell" id="cA"><button class="cb" id="bAuto">AUTOMATIC</button></div>
<div class="cell" id="cT"><button class="cb" id="bTuner">INT TUNER</button></div>
<div class="cell" id="cU"><button class="cb" id="bTune">TUNING</button></div>
</div>
</div>
<div class="ants" id="ants"></div>
</main>

<main id="set" hidden>
<h2 style="margin-top:0">Antenna amount</h2>
<div class="row"><input type="number" id="nAnt" min="1" max="8" value="8" style="width:4.5rem;flex:none"></div>
<h2>Antenna names</h2>
<div id="antRows"></div>
<h2>Band to antenna assignment</h2>
<div class="grid2" id="bandRows"></div>
<h2>ICOM only</h2>
<div class="row"><label class="l">TRX type</label><select id="trx" style="max-width:14rem"></select></div>
<div class="btns">
<button onclick="loadCfg(true)">Get Config</button>
<button class="go" onclick="save()">Send Config</button>
<button onclick="reboot()">Reboot</button>
<button class="warn" onclick="resetUnit()">Reset</button>
</div>
<div id="msg"></div>
</main>

<main id="upd" hidden>
<h2 style="margin-top:0">Firmware update</h2>
<p>Running firmware: <strong id="ver">-</strong></p>
<p class="note">Choose the app image <strong>Antenna-Remote_ota.bin</strong>, not the merged USB image. The unit restarts when the transfer is done. The antenna relays keep their state until then, and the antenna is set again right after the restart.</p>
<div class="row"><input type="file" id="fw" accept=".bin"></div>
<div class="btns"><button class="go" id="bFlash" onclick="flash()">Flash</button></div>
<progress id="prog" value="0" max="100" hidden style="width:100%;max-width:44rem;margin-top:.8rem"></progress>
<div id="umsg"></div>
</main>
<div id="toast" hidden></div>

<script>
const $=i=>document.getElementById(i);
const BANDS=[160,80,60,40,30,20,17,15,12,10,6,4];
const TRX=[["None",0],["IC-7000",112],["IC-7300",148],["IC-7610",152]];
let S=null,antKey='',built=false,uploading=false,toastT=0;

async function call(path,body){
  const o=body===undefined?{}:{method:'POST',headers:{'X-AR':'1','Content-Type':'application/x-www-form-urlencoded'},body:body};
  const r=await fetch(path,o);
  if(!r.ok)throw new Error('HTTP '+r.status);
  return r.json();
}

async function poll(){
  if(!uploading){
    try{S=await call('/api/state');render();$('conn').textContent='connected';$('conn').className='';}
    catch(e){$('conn').textContent='no connection';$('conn').className='bad';}
  }
  setTimeout(poll,600);
}

async function act(path,body){
  try{S=await call(path,body);render();}catch(e){}
}

function toast(t){
  const e=$('toast');e.textContent=t;e.hidden=false;
  clearTimeout(toastT);toastT=setTimeout(()=>{e.hidden=true;},7000);
}

function len(cellId,text){$(cellId).style.setProperty('--len',Math.max(3,text.length));}

function render(){
  const s=S;
  $('ver').textContent=s.ver;
  $('hver').textContent='FW '+s.ver;
  document.querySelectorAll('#link .bars i').forEach((e,k)=>e.classList.toggle('on',k<s.bars));
  $('rssi').textContent=s.rssi+' dBm';
  $('sdr').classList.toggle('on',!!s.sdr);
  $('sdr').title=s.sdr?'SDROxide is connected':'SDROxide is not connected';
  $('qrg').textContent=s.qrg>0?(s.qrg/1e6).toFixed(4):'-.----';
  $('sub').textContent='BAND '+(s.band?s.band+' m':'--')+(s.name?'  |  '+s.name:'');
  const key=s.n+'|'+s.names.slice(0,s.n).join('\u0001');
  if(key!==antKey){
    antKey=key;
    const box=$('ants');box.textContent='';
    for(let i=1;i<=s.n;i++){
      const c=document.createElement('div');c.className='cell';c.id='cell'+i;
      c.style.setProperty('--len',Math.max(3,s.names[i-1].length));
      const b=document.createElement('button');
      b.className='ant';b.id='ant'+i;b.textContent=s.names[i-1];
      b.onclick=()=>act('/api/ant','n='+i);
      c.appendChild(b);box.appendChild(c);
    }
  }
  for(let i=1;i<=s.n;i++){const b=$('ant'+i);b.classList.toggle('sel',s.ant===i);b.disabled=!!s.auto;}
  const a=$('bAuto');
  a.textContent=s.auto?'AUTOMATIC ON':'AUTOMATIC OFF';
  a.classList.toggle('on',!!s.auto);
  a.onclick=()=>act('/api/auto','v='+(s.auto?0:1));
  const tr=$('bTuner'),tu=$('bTune');
  tr.textContent=s.tuneExt?'INT TUNER OFF':'INT TUNER ON';
  tr.disabled=!!s.auto||s.trx===0;
  tr.onclick=()=>act('/api/tuner','v='+(s.tuneExt?0:1));
  tu.textContent=s.tuning?'TUNING ACTIVE':'TUNING INACTIVE';
  tu.classList.toggle('red',!!s.tuning);
  tu.disabled=s.trx===0||!s.tuneExt;
  tu.onclick=()=>act('/api/tune','v='+(s.tuning?0:1));
  len('cA',a.textContent);len('cT',tr.textContent);len('cU',tu.textContent);
}

function tab(t){
  $('main').hidden=t!=='main';$('set').hidden=t!=='set';$('upd').hidden=t!=='upd';
  $('tabMain').classList.toggle('on',t==='main');$('tabSet').classList.toggle('on',t==='set');$('tabUpd').classList.toggle('on',t==='upd');
  if(t==='set'){build();loadCfg(false);}
}

function msg(t,err){const m=$('msg');m.textContent=t;m.className=err?'err':'';}

function build(){
  if(built)return;built=true;
  const ar=$('antRows');
  for(let i=0;i<8;i++){
    const r=document.createElement('div');r.className='row';
    r.innerHTML='<label class="l">Antenna '+(i+1)+'</label><input type="text" id="nm'+i+'" maxlength="20"><label class="ext"><input type="checkbox" id="ex'+i+'"> External Tuner</label>';
    ar.appendChild(r);
    r.querySelector('input[type=text]').oninput=function(){this.value=this.value.replace(/[^\x20-\x7E]/g,'');bandOptions();};
  }
  const br=$('bandRows');
  for(let j=0;j<12;j++){
    const r=document.createElement('div');r.className='row';
    r.innerHTML='<label class="l">'+BANDS[j]+'m</label><select id="bd'+j+'"></select>';
    br.appendChild(r);
  }
  const t=$('trx');
  TRX.forEach(x=>{const o=document.createElement('option');o.value=x[1];o.textContent=x[0];t.appendChild(o);});
  bandOptions();
}

function bandOptions(){
  for(let j=0;j<12;j++){
    const sel=$('bd'+j),cur=sel.value||'1';
    sel.textContent='';
    for(let i=0;i<8;i++){
      const o=document.createElement('option');
      o.value=i+1;o.textContent=$('nm'+i).value||('Antenna '+(i+1));
      sel.appendChild(o);
    }
    sel.value=cur;
  }
}

function fill(c){
  $('nAnt').value=c.n;
  for(let i=0;i<8;i++){$('nm'+i).value=c.names[i];$('ex'+i).checked=!!((c.tuneroff>>i)&1);}
  bandOptions();
  for(let j=0;j<12;j++)$('bd'+j).value=c.bands[j];
  const t=$('trx');
  if(![...t.options].some(o=>+o.value===c.trx)){const o=document.createElement('option');o.value=c.trx;o.textContent='Custom ('+c.trx+')';t.appendChild(o);}
  t.value=c.trx;
}

async function loadCfg(explicit){
  try{fill(await call('/api/config'));if(explicit)msg('Config read from the unit');}
  catch(e){msg('Could not read the config',true);}
}

async function save(){
  const n=+$('nAnt').value;
  if(!(n>=1&&n<=8)){msg('Antenna amount must be 1 to 8',true);return;}
  for(let i=0;i<8;i++)if(!$('nm'+i).value.trim()){msg('Every antenna needs a name',true);return;}
  const p=new URLSearchParams();
  p.set('n',n);p.set('trx',$('trx').value);
  let t=0;for(let i=0;i<8;i++)if($('ex'+i).checked)t|=1<<i;
  p.set('tuneroff',t);
  for(let i=0;i<8;i++)p.set('nm'+i,$('nm'+i).value.trim());
  for(let j=0;j<12;j++)p.set('b'+j,$('bd'+j).value);
  try{fill(await call('/api/config',p.toString()));msg('Config sent to the unit');}
  catch(e){msg('Sending failed',true);}
}

async function reboot(){
  if(!confirm('Reboot the unit?'))return;
  try{await call('/api/reboot','x=1');}catch(e){}
  msg('Rebooting - the page reconnects by itself');
}

async function resetUnit(){
  if(!confirm('Reset the unit to its default values? Antenna names, band assignment and tuner settings are lost.'))return;
  try{await call('/api/reset','x=1');}catch(e){}
  msg('Reset done, the unit reboots');
}

function um(t,err){const m=$('umsg');m.textContent=t;m.className=err?'err':'';}

async function waitBack(oldVer){
  for(let i=0;i<90;i++){
    await new Promise(r=>setTimeout(r,1000));
    try{
      const s=await call('/api/state');
      S=s;render();
      uploading=false;$('bFlash').disabled=false;$('prog').hidden=true;
      if(s.ver!==oldVer){um('');tab('main');toast('Update done. Now running '+s.ver);}
      else um('The unit is back, but still runs '+s.ver,true);
      return;
    }catch(e){}
  }
  uploading=false;$('bFlash').disabled=false;
  um('The unit did not come back within 90 s',true);
}

function flash(){
  const f=$('fw').files[0];
  if(!f){um('Choose a .bin file first',true);return;}
  if(!confirm('Flash '+f.name+'? The unit restarts afterwards.'))return;
  const old=S?S.ver:'';
  const fd=new FormData();fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest();
  x.open('POST','/update');x.setRequestHeader('X-AR','1');
  uploading=true;$('bFlash').disabled=true;
  x.upload.onprogress=e=>{if(e.lengthComputable){$('prog').hidden=false;$('prog').value=e.loaded*100/e.total;}};
  x.onload=()=>{
    if(x.status===200){um('Transfer done, the unit restarts...');waitBack(old);}
    else{uploading=false;$('bFlash').disabled=false;um('Update failed: '+x.responseText,true);}
  };
  x.onerror=()=>{um('Connection lost, waiting for the unit...',true);waitBack(old);};
  um('Uploading...');
  x.send(fd);
}

poll();
</script>
</body>
</html>
)=====";
