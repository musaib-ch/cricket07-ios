/* Lifecycle of one private game instance. Recovery always creates a fresh page. */
(() => {
  let state='idle',lastFrame=0,started=0,warning=false;
  const log=[];
  const post=message=>window.webkit?.messageHandlers?.boundary?.postMessage(message);
  function report(message){post({type:'error',message,log:log.slice(-12)})}
  window.BoundarySession={
    begin(){
      if(state!=='idle')return false;
      state='starting';started=performance.now();lastFrame=0;warning=false;
      document.querySelector('#play')?.setAttribute('disabled','');
      post({type:'starting'});return true;
    },
    frame(){
      if(state==='failed')return;
      lastFrame=performance.now();
      if(state==='starting'||warning){state='running';warning=false;post({type:'first-frame'})}
    },
    log(text){
      log.push(String(text).slice(0,500));if(log.length>30)log.shift();
      if(/\[wgpu\] (device|adapter):|out of memory|memory access out of bounds|Aborted\(/i.test(text))this.fail(Error(String(text)));
    },
    fail(error){
      if(state==='failed')return;state='failed';window.BoundaryControls?.reset();
      const message=String(error?.message||error||'Game startup failed.');report(message);
      let panel=document.querySelector('#b07-recovery');
      if(!panel){panel=document.createElement('section');panel.id='b07-recovery';panel.setAttribute('role','alert');document.body.append(panel)}
      panel.replaceChildren();const title=document.createElement('h2');title.textContent='The game stopped';
      const detail=document.createElement('p');detail.textContent=message;
      const retry=document.createElement('button');retry.textContent='Restart in Performance mode';retry.onclick=()=>{const url=new URL(location.href);url.searchParams.set('safe','1');location.replace(url)};
      panel.append(title,detail,retry);
    },
    get active(){return state==='starting'||state==='running'},
    get state(){return state}
  };
  setInterval(()=>{
    if(document.hidden||warning||!BoundarySession.active)return;
    const elapsed=performance.now()-(lastFrame||started),limit=state==='starting'?90000:30000;
    if(elapsed>limit){warning=true;report(state==='starting'?'No game frame has appeared yet. Restart in Performance mode if this screen stays black.':'The game has stopped presenting frames. You can restart it; unsaved match progress will be lost.')}
  },5000);
  addEventListener('unhandledrejection',event=>{if(BoundarySession.active)BoundarySession.fail(event.reason)});
  addEventListener('error',event=>{if(BoundarySession.active)BoundarySession.fail(Error(event.message))});
  addEventListener('pagehide',()=>window.BoundaryControls?.reset());
  document.addEventListener('visibilitychange',()=>{if(!document.hidden){lastFrame=performance.now();started=lastFrame}});
})();
