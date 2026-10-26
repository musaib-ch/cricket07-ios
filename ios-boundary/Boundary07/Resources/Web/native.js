'use strict';
window.BoundaryNative={
  async prepare(){
    const report=await BoundaryCapabilities.run();
    if(!report.allRequiredPassed)throw Error('This WebKit context is missing a required feature. Return to the pavilion and export the device report.');
    const response=await fetch('native-manifest.json');
    if(!response.ok)throw Error('Import your private bundle in the native pavilion first.');
    const manifest=await response.json();
    const root=await navigator.storage.getDirectory();
    let previous;
    try{const dir=await root.getDirectoryHandle('rcgame');previous=JSON.parse(await(await(await dir.getFileHandle('.installed.json')).getFile()).text())}catch{}
    if(previous?.version===manifest.version)return;
    const estimate=await navigator.storage.estimate();
    const total=manifest.files.reduce((n,f)=>n+f.bytes,0);
    if(estimate.quota&&estimate.quota-(estimate.usage||0)<total+50*1024*1024)throw Error('There is not enough private browser storage for the game import. Free space on this device and try again.');
    const status=document.getElementById('status'),progress=document.getElementById('progress');
    progress.classList.remove('hidden');
    try{await root.removeEntry('rcgame',{recursive:true})}catch{}
    const game=await root.getDirectoryHandle('rcgame',{create:true});let done=0;
    for(const file of manifest.files){
      const parts=file.path.split('/');
      if(parts.some(p=>!p||p==='.'||p==='..'||p.includes('\\')))throw Error('Invalid asset path in imported bundle');
      status.textContent=`Preparing ${file.path} · ${Math.round(done/1e6)} / ${Math.round(total/1e6)} MB`;
      let dir=game;for(const part of parts.slice(0,-1))dir=await dir.getDirectoryHandle(part,{create:true});
      const writer=await(await dir.getFileHandle(parts.at(-1),{create:true})).createWritable();
      try{
        const source=await fetch('game/'+parts.map(encodeURIComponent).join('/'));if(!source.ok)throw Error('Cannot read '+file.path);
        const reader=source.body.getReader();let bytes=0;
        for(;;){const chunk=await reader.read();if(chunk.done)break;await writer.write(chunk.value);bytes+=chunk.value.byteLength;done+=chunk.value.byteLength;progress.value=done/total}
        if(bytes!==file.bytes)throw Error('Incomplete asset transfer: '+file.path);await writer.close();
      }catch(e){await writer.abort().catch(()=>{});throw e}
    }
    const marker=await(await game.getFileHandle('.installed.json',{create:true})).createWritable();
    await marker.write(JSON.stringify({version:manifest.version,at:new Date().toISOString()}));await marker.close();
    progress.classList.add('hidden');
    document.querySelector('#reset').textContent='Import another private bundle';
  }
};
const allowedTouch=new Set(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','KeyS','KeyD','KeyA','KeyW','ShiftLeft','Escape']);
window.boundaryTouch=(code,down)=>{if(!allowedTouch.has(code)||!window.recomp||document.querySelector('dialog[open]'))return;const event=new KeyboardEvent(down?'keydown':'keyup',{code,bubbles:true});window.dispatchEvent(event)};
window.addEventListener('error',event=>window.webkit?.messageHandlers?.boundary?.postMessage({type:'error',message:event.message}));
