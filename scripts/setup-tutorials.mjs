// Requires Node.js 22+ and Windows PowerShell. Downloads are checksum locked.
import {readFile,writeFile,mkdir,rm} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const catalog=JSON.parse(await readFile(path.join(root,'assets/music/tutorials.json'),'utf8'));
const hash=b=>createHash('sha256').update(b).digest('hex');
function vlq(n){const b=[n&127];while(n>>=7)b.unshift((n&127)|128);return b;}
function midi(notes,tempo){
 const events=[[0,0,[255,81,3,...[16,8,0].map(s=>(Math.round(60000000/tempo)>>s)&255)]],[0,0,[255,88,4,4,2,24,8]]];
 for(const [tick,duration,pitch,channel=0] of notes){events.push([tick,2,[144|channel,pitch,80]],[tick+duration,1,[128|channel,pitch,0]]);}
 events.sort((a,b)=>a[0]-b[0]||a[1]-b[1]);let last=0;const bytes=[];
 for(const [tick,,data] of events){bytes.push(...vlq(tick-last),...data);last=tick;}bytes.push(0,255,47,0);
 const size=Buffer.alloc(4);size.writeUInt32BE(bytes.length);
 return Buffer.concat([Buffer.from('4d546864000000060000000101e04d54726b','hex'),size,Buffer.from(bytes)]);
}
async function download(url){const r=await fetch(url);if(!r.ok)throw Error(`${r.status}: ${url}`);return Buffer.from(await r.arrayBuffer());}
const archives=new Map();
await mkdir(path.join(root,'assets/music/tutorials'),{recursive:true});
for(const entry of catalog){
 const dest=path.join(root,'assets/music',entry.file);let current;
 try{current=await readFile(dest);}catch{}
 if(current&&hash(current)===entry.download.sha256)continue;
 let data;
 if(entry.download.notes)data=midi(entry.download.notes,entry.tutorial.startTempo);
 else if(entry.download.archiveMember){
  const url=entry.download.url;
  if(!archives.has(url)){
   const bytes=await download(url);if(hash(bytes)!==entry.download.archiveSha256)throw Error('Archive checksum mismatch');
   const cache=path.join(root,'.cache/tutorials-setup');await mkdir(cache,{recursive:true});
   const zip=path.join(cache,'hanon.zip'),dir=path.join(cache,'hanon');await writeFile(zip,bytes);
   execFileSync('powershell.exe',['-NoProfile','-Command',`Expand-Archive -LiteralPath '${zip.replaceAll("'","''")}' -DestinationPath '${dir.replaceAll("'","''")}' -Force`]);
   archives.set(url,dir);
  }
  data=await readFile(path.join(archives.get(url),entry.download.archiveMember));
 }else data=await download(entry.download.url);
 if(hash(data)!==entry.download.sha256)throw Error(`Checksum mismatch: ${entry.id}; catalog update requires source review`);
 if(data.subarray(0,4).toString()!=='MThd')throw Error(`Invalid MIDI: ${entry.id}`);
 await writeFile(dest,data);console.log(`Installed ${entry.title}`);
}
await rm(path.join(root,'.cache/tutorials-setup'),{recursive:true,force:true});
console.log(`Verified ${catalog.length} tutorial MIDI files.`);
