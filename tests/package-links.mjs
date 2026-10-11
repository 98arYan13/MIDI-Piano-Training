import assert from 'node:assert/strict';
import {readFile,stat} from 'node:fs/promises';
import {resolve,dirname,sep} from 'node:path';
export async function checkPortableDocLinks(folder){
 const root=resolve(folder),pending=['README.md','docs/USER-GUIDE.md','docs/RELEASE-VERIFICATION.md'],seen=new Set();
 while(pending.length){const file=resolve(root,pending.pop());if(seen.has(file))continue;seen.add(file);const text=await readFile(file,'utf8');
  for(const match of text.matchAll(/\[[^\]]*\]\(([^)]+)\)/g)){const target=match[1].split('#')[0];if(!target||/^[a-z]+:/i.test(target))continue;const path=resolve(dirname(file),decodeURIComponent(target));assert(path.startsWith(root+sep),'Portable link escapes package '+target);assert((await stat(path).catch(()=>null))?.isFile(),'Missing portable document link '+target+' from '+file);if(path.endsWith('.md')&&!path.includes(sep+'verification'+sep))pending.push(path);}
 }
 return seen.size;
}
if(process.argv[1]?.endsWith('package-links.mjs'))console.log(`PASS ${await checkPortableDocLinks(process.env.SR_RELEASE??'build/release/SightReading')} portable documents and their internal links`);
