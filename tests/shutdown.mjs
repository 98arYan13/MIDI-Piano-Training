import assert from 'node:assert/strict';
const url=process.env.SR_URL ?? 'http://127.0.0.1:18781';
assert.equal((await fetch(url+'/api/v1/shutdown',{method:'POST'})).status,403);
assert.equal((await fetch(url+'/api/v1/shutdown',{method:'POST',headers:{Origin:url}})).status,200);
for(let i=0;i<100;i++){try{await fetch(url+'/api/v1/status');}catch{console.log('PASS origin-restricted graceful native shutdown');process.exit(0);}await new Promise(r=>setTimeout(r,100));}
throw Error('Service did not shut down');
