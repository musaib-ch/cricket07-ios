/* Boundary 07 touch controls. Pointer IDs own inputs independently; no game assets. */
(() => {
  'use strict';
  const HID={ArrowUp:82,ArrowDown:81,ArrowLeft:80,ArrowRight:79,KeyS:22,KeyD:7,KeyA:4,KeyW:26,ShiftLeft:225,Escape:41,Tab:43};
  const owners=new Map(),held=new Set(),pointers=new Map();
  let stickOwner=null,loft=false,mouseTimer,mouseDown=false,overlay;
  const runtime=()=>window.recomp;
  function sync(){
    const next=new Set([...owners.values()].flatMap(keys=>[...keys]));
    for(const key of held)if(!next.has(key)){runtime()?._input_key_hid(HID[key],0);held.delete(key)}
    for(const key of next)if(!held.has(key)&&runtime()){runtime()._input_key_hid(HID[key],1);held.add(key)}
  }
  function own(id,keys){owners.set(id,new Set(keys));sync()}
  function release(id){owners.delete(id);sync()}
  // Eight keyboard directions, including two held keys for diagonal shots.
  function directions(x,y){
    if(Math.hypot(x,y)<0.20)return [];
    const angle=Math.atan2(y,x),sector=Math.round(angle/(Math.PI/4));
    return ({'-4':['ArrowLeft'],'-3':['ArrowLeft','ArrowUp'],'-2':['ArrowUp'],'-1':['ArrowUp','ArrowRight'],0:['ArrowRight'],1:['ArrowRight','ArrowDown'],2:['ArrowDown'],3:['ArrowDown','ArrowLeft'],4:['ArrowLeft']})[sector];
  }
  function setLoft(value){
    loft=value;own('loft',loft?['ShiftLeft']:[]);
    const button=overlay?.querySelector('[data-action="loft"]');
    if(button){button.classList.toggle('on',loft);button.setAttribute('aria-pressed',String(loft));button.textContent='LOFT '+(loft?'ON':'OFF')}
  }
  function reset(){
    pointers.clear();stickOwner=null;owners.clear();sync();setLoft(false);
    clearTimeout(mouseTimer);if(mouseDown)runtime()?._input_mouse_button(0,0);mouseDown=false;
    overlay?.querySelectorAll('.pressed').forEach(el=>el.classList.remove('pressed'));
    const thumb=overlay?.querySelector('.b07-thumb');if(thumb)thumb.style.transform='translate(-50%,-50%)';
  }
  function click(){
    clearTimeout(mouseTimer);if(mouseDown)runtime()?._input_mouse_button(0,0);
    runtime()?._input_mouse_button(0,1);mouseDown=true;
    mouseTimer=setTimeout(()=>{runtime()?._input_mouse_button(0,0);mouseDown=false},90);
  }
  let remainderX=0,remainderY=0;
  function move(dx,dy){
    if(!Number.isFinite(dx)||!Number.isFinite(dy))return;
    // Cricket's menu coordinates use a 1024 x 768 game window, independent of render quality.
    const r=document.querySelector('#canvas').getBoundingClientRect();
    remainderX+=dx*1024/Math.max(r.width,1);remainderY+=dy*768/Math.max(r.height,1);
    const x=Math.trunc(remainderX),y=Math.trunc(remainderY);remainderX-=x;remainderY-=y;
    runtime()?._input_mouse_move(x,y);
  }
  function updateStick(event){
    const pad=overlay.querySelector('.b07-stick'),r=pad.getBoundingClientRect(),radius=r.width/2;
    let x=(event.clientX-r.left-radius)/radius,y=(event.clientY-r.top-radius)/radius;
    const length=Math.hypot(x,y);if(length>1){x/=length;y/=length}
    own(event.pointerId,directions(x,y));
    pad.querySelector('.b07-thumb').style.transform=`translate(calc(-50% + ${x*radius*.62}px),calc(-50% + ${y*radius*.62}px))`;
  }
  function finish(event,cancelled=false){
    const p=pointers.get(event.pointerId);if(!p)return;
    if(p.kind==='pointer'&&!cancelled&&performance.now()-p.time<400&&p.travel<9)click();
    if(p.kind==='stick'){
      stickOwner=null;overlay.querySelector('.b07-thumb').style.transform='translate(-50%,-50%)';
    }
    release(event.pointerId);p.element?.classList.remove('pressed');pointers.delete(event.pointerId);
  }
  function mount(){
    if(overlay)return;
    overlay=document.createElement('section');overlay.id='b07-controls';overlay.setAttribute('aria-label','Touch controller');
    overlay.innerHTML=`<div class="b07-trackpad" aria-label="Mouse trackpad: drag to move, tap to click"><span>DRAG TO MOVE POINTER · TAP TO CLICK</span></div>
      <button class="b07-menu-button" data-action="menu" aria-expanded="false" aria-label="Options">☰</button><div class="b07-settings" hidden><button data-action="visibility">Hide controls</button><button data-action="saves">Saves</button><label>Screen <select id="b07-fit"><option value="fit">Fit · 4:3</option><option value="fill">Fill screen</option></select></label></div>
      <button class="b07-loft" data-action="loft" aria-pressed="false">LOFT OFF</button>
      <div class="b07-stick" role="group" aria-label="Direction stick"><span class="b07-cross">✛</span><div class="b07-thumb"></div></div>
      <div class="b07-face"><button class="b07-y" data-key="KeyW" aria-label="Y, W">Y<small>W</small></button><button class="b07-x" data-key="KeyA" aria-label="X, A">X<small>A</small></button><button class="b07-b" data-key="KeyD" aria-label="B, D">B<small>D</small></button><button class="b07-a" data-key="KeyS" aria-label="A, S">A<small>S</small></button></div>
      <div class="b07-system"><button data-key="Tab">SELECT</button><button data-key="Escape">START</button></div>`;
    document.body.append(overlay);
    overlay.addEventListener('pointerdown',event=>{
      if(event.target.closest('.b07-settings'))return;
      event.preventDefault();event.stopPropagation();
      if(!runtime()||document.querySelector('dialog[open]'))return;
      const element=event.target.closest('[data-key],.b07-stick,.b07-trackpad,[data-action="loft"]');if(!element)return;
      if(element.dataset.action==='loft'){setLoft(!loft);return}
      let kind=element.dataset.key?'key':element.classList.contains('b07-stick')?'stick':'pointer';
      if(kind==='stick'&&stickOwner!==null)return;
      element.setPointerCapture(event.pointerId);
      pointers.set(event.pointerId,{kind,element,x:event.clientX,y:event.clientY,time:performance.now(),travel:0});
      if(kind==='stick'){stickOwner=event.pointerId;updateStick(event)}
      if(kind==='key'){own(event.pointerId,[element.dataset.key]);element.classList.add('pressed')}
    });
    overlay.addEventListener('pointermove',event=>{
      const p=pointers.get(event.pointerId);if(!p)return;event.preventDefault();event.stopPropagation();
      if(p.kind==='stick')updateStick(event);
      if(p.kind==='pointer'){const dx=event.clientX-p.x,dy=event.clientY-p.y;p.travel+=Math.hypot(dx,dy);move(dx,dy);p.x=event.clientX;p.y=event.clientY}
    });
    overlay.addEventListener('pointerup',event=>finish(event));
    overlay.addEventListener('pointercancel',event=>finish(event,true));
    overlay.addEventListener('lostpointercapture',event=>finish(event,true));
    overlay.addEventListener('contextmenu',event=>event.preventDefault());
    overlay.querySelector('[data-action="visibility"]').onclick=()=>{
      reset();const hidden=overlay.classList.toggle('controls-hidden');overlay.querySelector('[data-action="visibility"]').textContent=hidden?'Show controls':'Hide controls';
    };
    overlay.querySelector('[data-action="saves"]').onclick=()=>window.BoundarySaves?.showTools();
    const fit=overlay.querySelector('#b07-fit');
    try{fit.value=localStorage.getItem('b07-fit')||'fit'}catch{}
    if(!['fit','fill'].includes(fit.value))fit.value='fit';
    if(new URLSearchParams(location.search).has('safe'))fit.value='fit';
    function settings(){
      reset();document.body.classList.toggle('b07-fill',fit.value==='fill');
      window.dispatchEvent(new Event('resize'));
      try{localStorage.setItem('b07-fit',fit.value)}catch{}
    }
    fit.onchange=settings;settings();
    overlay.querySelector('[data-action="menu"]').onclick=()=>{
      const menu=overlay.querySelector('.b07-settings'),open=menu.hasAttribute('hidden');
      if(open)menu.removeAttribute('hidden');else menu.setAttribute('hidden','');
      overlay.querySelector('[data-action="menu"]').setAttribute('aria-expanded',String(open));
    };
    setTimeout(()=>overlay.querySelector('.b07-trackpad span')?.classList.add('faded'),3500);
  }
  function viewport(){
    document.documentElement.style.setProperty('--b07-w',innerWidth+'px');document.documentElement.style.setProperty('--b07-h',innerHeight+'px');
  }
  let quality=720;try{const value=Number(localStorage.getItem('b07-quality'));if([720,960,1200].includes(value))quality=value}catch{}
  if(new URLSearchParams(location.search).has('safe'))quality=720;
  window.BoundaryViewport={height:quality};
  window.BoundaryControls={reset,directions,start:mount};
  window.boundaryTouch=(code,down)=>{if(HID[code]){if(down)own('legacy:'+code,[code]);else release('legacy:'+code)}};
  window.addEventListener('blur',reset);window.addEventListener('pagehide',reset);
  document.addEventListener('visibilitychange',()=>{if(document.hidden)reset()});
  new MutationObserver(()=>{if(document.querySelector('dialog[open]'))reset()}).observe(document.body,{subtree:true,attributes:true,attributeFilter:['open']});
  for(const name of ['gesturestart','gesturechange','gestureend'])document.addEventListener(name,event=>{if(document.body.classList.contains('playing'))event.preventDefault()},{passive:false});
  document.addEventListener('dblclick',event=>{if(document.body.classList.contains('playing'))event.preventDefault()},{passive:false});
  window.addEventListener('resize',viewport);window.visualViewport?.addEventListener('resize',viewport);viewport();
})();
