/* Persistent guest user data only. This is a file backup, never a live RAM snapshot. */
(() => {
  const FORMAT='boundary07-saves-v1',LIMIT=32*1024*1024,FILE_LIMIT=8*1024*1024;
  const valid=path=>typeof path==='string'&&path.length<1024&&!path.includes('\\')&&!path.includes('\0')&&path.split('/').every(p=>p&&p!=='.'&&p!=='..'&&!p.includes(':'));
  const root=()=>navigator.storage.getDirectory();
  async function list(directory,prefix='',out=[]){
    for await(const [name,handle] of directory.entries()){
      const path=prefix+name;if(!valid(path))throw Error('Invalid saved file path.');
      if(handle.kind==='directory')await list(handle,path+'/',out);
      else{const file=await handle.getFile();out.push({path,handle,bytes:file.size});if(out.length>1000)throw Error('Too many saved files.')}
    }return out.sort((a,b)=>a.path.localeCompare(b.path));
  }
  async function inventory(){
    try{return await list(await(await root()).getDirectoryHandle('userdata'))}
    catch(error){if(error.name==='NotFoundError')return [];throw error}
  }
  const encode=bytes=>{let text='';for(let i=0;i<bytes.length;i+=16384)text+=String.fromCharCode(...bytes.subarray(i,i+16384));return btoa(text)};
  function validate(backup){
    if(backup?.format!==FORMAT||!Array.isArray(backup.files)||backup.files.length>1000)throw Error('Select a Boundary 07 saved-progress backup.');
    let total=0;const seen=new Set();
    return backup.files.map(file=>{
      if(!valid(file.path)||seen.has(file.path)||typeof file.base64!=='string'||file.base64.length>Math.ceil(FILE_LIMIT/3)*4)throw Error('Invalid saved-progress backup.');
      seen.add(file.path);const raw=atob(file.base64);total+=raw.length;if(total>LIMIT)throw Error('Saved progress exceeds 32 MB.');
      return {path:file.path,bytes:Uint8Array.from(raw,c=>c.charCodeAt(0))};
    });
  }
  async function backup(){
    const files=await inventory();let total=0;const result=[];
    for(const file of files){total+=file.bytes;if(file.bytes>FILE_LIMIT||total>LIMIT)throw Error('Saved progress exceeds the backup limit.');result.push({path:file.path,base64:encode(new Uint8Array(await(await file.handle.getFile()).arrayBuffer()))})}
    return {format:FORMAT,at:new Date().toISOString(),files:result};
  }
  async function writeFiles(files){
    const storage=await root();try{await storage.removeEntry('userdata',{recursive:true})}catch(e){if(e.name!=='NotFoundError')throw e}
    const directory=await storage.getDirectoryHandle('userdata',{create:true});
    for(const file of files){let dir=directory;const parts=file.path.split('/');for(const part of parts.slice(0,-1))dir=await dir.getDirectoryHandle(part,{create:true});const writer=await(await dir.getFileHandle(parts.at(-1),{create:true})).createWritable();try{await writer.write(file.bytes);await writer.close()}catch(e){await writer.abort().catch(()=>{});throw e}}
  }
  async function restore(value){
    if(window.recomp||window.BoundarySession?.active)throw Error('Return to the pavilion and open Saved progress before restoring.');
    const incoming=validate(value);if(!incoming.length)throw Error('This backup contains no saved files.');
    const previous=validate(await backup());
    try{await writeFiles(incoming)}catch(error){try{await writeFiles(previous)}catch{throw Error('Restore and rollback both failed. Keep your backup and free storage before retrying.')}throw error}
  }
  async function prepare(){
    const directory=await(await root()).getDirectoryHandle('userdata',{create:true});
    const name='.boundary-write-check';const handle=await directory.getFileHandle(name,{create:true});const writer=await handle.createWritable();await writer.write('ok');await writer.close();await directory.removeEntry(name);
    try{await navigator.storage.persist?.()}catch{}
  }
  async function exportBackup(){
    const data=await backup();if(!data.files.length)throw Error('No saved files yet. Create a profile or save progress inside Cricket 07 first.');
    const native=window.webkit?.messageHandlers?.boundary;if(!native)throw Error('Export this backup from the Boundary 07 app.');
    native.postMessage({type:'save-backup',backup:data});
    return data.files.length;
  }
  const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  let typing=false;
  async function key(usage){const engine=window.recomp;if(!engine)throw Error('Start the game first.');engine._input_key_hid(usage,1);try{await wait(90)}finally{engine._input_key_hid(usage,0)}await wait(70)}
  async function typeName(text){
    if(typing)throw Error('Wait for the previous name to finish.');
    if(!/^[A-Za-z0-9 ]{1,24}$/.test(text))throw Error('Use 1–24 letters, numbers or spaces.');
    typing=true;window.BoundaryControls?.reset();
    try{for(const ch of text.toLowerCase()){if(document.hidden)throw Error('Typing stopped because the app went into the background.');const n=ch.charCodeAt(0);await key(n>=97&&n<=122?n-97+4:ch===' '?44:ch==='0'?39:Number(ch)+29)}}finally{typing=false}
  }
  function showTools(){
    let dialog=document.querySelector('#b07-save-dialog');
    if(!dialog){
      dialog=document.createElement('dialog');dialog.id='b07-save-dialog';
      dialog.innerHTML='<form method="dialog"><button class="b07-close">Close</button></form><h2>Profiles & saved progress</h2><p>Use Cricket 07’s profile and save/load menus to save supported progress. Continuing without a profile does not create one. A file backup cannot resume an unsaved match after the app closes.</p><p>Select the game’s name field first, then enter your profile or save name here:</p><form id="b07-name-form"><input id="b07-name" aria-label="Profile or save name" maxlength="24" autocomplete="off" autocapitalize="off" placeholder="Profile name"><button>Send name to game</button></form><button id="b07-erase">Backspace in game</button> <button id="b07-enter">Enter in game</button><p id="b07-save-status" role="status"></p><button id="b07-export">Export saved progress</button>';
      document.body.append(dialog);
      dialog.querySelector('#b07-name-form').onsubmit=async event=>{event.preventDefault();const text=dialog.querySelector('#b07-name').value;dialog.close();try{await wait(300);await typeName(text)}catch(e){alert(e.message)}};
      for(const [id,usage] of [['#b07-erase',42],['#b07-enter',40]])dialog.querySelector(id).onclick=async()=>{dialog.close();await wait(300);await key(usage)};
      dialog.querySelector('#b07-export').onclick=async()=>{const status=dialog.querySelector('#b07-save-status');try{status.textContent='Preparing local backup…';const n=await exportBackup();status.textContent=`Backup ready: ${n} files. Use Share backup at the top of the app.`}catch(e){status.textContent=e.message}};
    }
    window.BoundaryControls?.reset();dialog.showModal();
    inventory().then(files=>dialog.querySelector('#b07-save-status').textContent=`Saved files on this device: ${files.length}.`).catch(e=>dialog.querySelector('#b07-save-status').textContent=e.message);
  }
  window.BoundarySaves={prepare,inventory,backup,restore,exportBackup,typeName,showTools,validate};
})();
