# GTAiOS host

This iOS host mounts the user-provided converted `playgta5.com` folder without copying its large `data/` tree into the application. The Files folder is retained as a security-scoped bookmark and served only on `127.0.0.1` with COOP/COEP isolation, so the WebAssembly loader can fetch `/b/`, `/title/` and `/data/` resources from the same local origin.

The game folder remains private and is not included in this source archive or any CI artifact. A physical iPhone test is required for WebGPU, memory, audio, input and sustained playability.
