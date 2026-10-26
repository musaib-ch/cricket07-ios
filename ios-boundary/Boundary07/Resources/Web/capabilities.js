'use strict';
window.BoundaryCapabilities = {
  async run() {
    const report={schema:1,at:new Date().toISOString(),userAgent:navigator.userAgent,context:'WKWebView candidate',gameplay:'Not tested',checks:{}};
    async function test(name,fn){try{report.checks[name]={pass:true,detail:await fn()}}catch(e){report.checks[name]={pass:false,error:String(e)}}}
    const need=(value,message)=>{if(!value)throw Error(message)};
    await test('secureContext',()=>{need(isSecureContext,'Local origin is not a secure context');return true});
    await test('crossOriginIsolation',()=>{need(crossOriginIsolated,'Cross-origin isolation is unavailable');return true});
    await test('sharedWasmMemory',async()=>{
      const memory=new WebAssembly.Memory({initial:16,maximum:65536,shared:true});
      need(memory.buffer instanceof SharedArrayBuffer,'Shared memory is unavailable');
      await WebAssembly.compile(new Uint8Array([0,97,115,109,1,0,0,0]));
      return 'Shared memory with a 4 GiB maximum accepted; full game allocation not tested';
    });
    await test('webgpu',async()=>{
      need(navigator.gpu,'WebGPU is unavailable');const adapter=await navigator.gpu.requestAdapter();need(adapter,'No GPU adapter');
      const device=await adapter.requestDevice();try{
        device.pushErrorScope('validation');const canvas=document.createElement('canvas');canvas.width=32;canvas.height=32;
        const context=canvas.getContext('webgpu');need(context,'No WebGPU canvas');
        context.configure({device,format:navigator.gpu.getPreferredCanvasFormat()});
        const encoder=device.createCommandEncoder();const pass=encoder.beginRenderPass({colorAttachments:[{view:context.getCurrentTexture().createView(),clearValue:{r:0.8,g:0.96,b:0.35,a:1},loadOp:'clear',storeOp:'store'}]});
        pass.end();device.queue.submit([encoder.finish()]);await device.queue.onSubmittedWorkDone();
        const error=await device.popErrorScope();need(!error,error?.message);context.unconfigure();
        return {features:[...adapter.features],maxBufferSize:adapter.limits.maxBufferSize};
      }finally{device.destroy()}
    });
    await test('privateStorage',async()=>{
      const root=await navigator.storage.getDirectory();const name='boundary-probe-'+crypto.randomUUID();
      const file=await root.getFileHandle(name,{create:true});
      try{const writer=await file.createWritable();await writer.write('boundary07');await writer.close();need(await(await file.getFile()).text()==='boundary07','Storage readback failed')}
      finally{await root.removeEntry(name)}return await navigator.storage.estimate();
    });
    await test('workerSharedMemoryAndSyncStorage',async()=>{
      const code=`onmessage=async({data})=>{try{Atomics.add(new Int32Array(data),0,1);const root=await navigator.storage.getDirectory();const name='boundary-worker-'+Date.now();const f=await root.getFileHandle(name,{create:true});const h=await f.createSyncAccessHandle();try{h.write(new Uint8Array([7]),{at:0});h.flush();const a=new Uint8Array(1);h.read(a,{at:0});if(a[0]!==7)throw Error('Readback failed')}finally{h.close();await root.removeEntry(name)}postMessage({ok:true})}catch(e){postMessage({error:String(e)})}}`;
      const url=URL.createObjectURL(new Blob([code],{type:'text/javascript'}));let worker;const buffer=new SharedArrayBuffer(4);
      try{await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Worker timed out')),15000);worker=new Worker(url);
        worker.onmessage=({data})=>{clearTimeout(timer);data.ok?resolve():reject(Error(data.error))};worker.onerror=e=>{clearTimeout(timer);reject(Error(e.message))};worker.postMessage(buffer)});
        need(Atomics.load(new Int32Array(buffer),0)===1,'Shared buffer was not updated');return true;
      }finally{worker?.terminate();URL.revokeObjectURL(url)}
    });
    report.allRequiredPassed=Object.values(report.checks).every(c=>c.pass);
    window.boundaryCapabilities=report;
    window.webkit?.messageHandlers?.boundary?.postMessage({type:'capabilities',report});
    return report;
  }
};
