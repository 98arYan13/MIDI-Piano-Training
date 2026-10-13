import assert from 'node:assert/strict';
import {readFile,writeFile,rename} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {join} from 'node:path';

export async function checkFingeringPackage(root) {
 const pins=JSON.parse(await readFile(join(root,'fingering/dependencies.json'),'utf8'));
 const manifest=JSON.parse(await readFile(join(root,'release-manifest.json'),'utf8'));
 const files=new Map(manifest.files.map(f=>[f.path.replaceAll('\\','/'),f]));
 const required=['fingering/model.py','fingering/worker.py','fingering/requirements.lock','.tools/fingering/python/python.exe','.tools/fingering/python/python311.dll','.tools/fingering/python/python311.zip','.tools/fingering/python/Lib/site-packages/torch/lib/torch_cpu.dll','.tools/fingering/python/msvcp140.dll','docs/licenses/FINGERING-NOTICES.txt'];
 for(const dependency of ['filelock/__init__.py','typing_extensions.py','sympy/__init__.py','networkx/__init__.py','jinja2/__init__.py','fsspec/__init__.py','markupsafe/__init__.py','mpmath/__init__.py'])required.push('.tools/fingering/python/Lib/site-packages/'+dependency);
 for(const hand of ['left','right']) required.push('.tools/fingering/models/'+pins.checkpoints[hand].file);
 required.push('fingering/dependencies.json','.tools/fingering/python/MSVC-LICENSE.rtf','.tools/fingering/python/Lib/site-packages/torch/_C.cp311-win_amd64.pyd');
 for(const dll of Object.keys(pins.msvcRuntime.files))required.push('.tools/fingering/python/'+dll);
 for(const path of required) {
  const entry=files.get(path);assert(entry,`Missing fingering release manifest entry: ${path}`);
  const bytes=await readFile(join(root,path));
  assert.equal(createHash('sha256').update(bytes).digest('hex'),entry.sha256,`Fingering checksum mismatch: ${path}`);
 }
 for(const hand of ['left','right'])assert.equal(files.get('.tools/fingering/models/'+pins.checkpoints[hand].file).sha256,pins.checkpoints[hand].sha256);
 for(const [dll,hash] of Object.entries(pins.msvcRuntime.files))assert.equal(files.get('.tools/fingering/python/'+dll).sha256,hash);
 assert.equal(files.get('.tools/fingering/python/MSVC-LICENSE.rtf').sha256,pins.msvcRuntime.licenseSha256);
 for(const path of files.keys())assert(!/\/(__pycache__|downloads|pip|torch\/include)\/|\.(pyc|lib)$/.test(path),`Build-only/cache file shipped: ${path}`);
 assert.match(await readFile(join(root,'docs/licenses/THIRD-PARTY-NOTICES.txt'),'utf8'),/Automatic Piano Fingering/);
 return {requiredFiles:required.length};
}

export async function checkFingeringPackageFailures(root) {
 const source=join(root,'fingering/worker.py'),original=await readFile(source);
 try {
  await writeFile(source,Buffer.concat([original,Buffer.from('\n# checksum fault\n')]));
  await assert.rejects(checkFingeringPackage(root),/checksum mismatch/);
 } finally {await writeFile(source,original);}
 for(const relative of ['.tools/fingering/models/left_ArGNN.pth','.tools/fingering/models/right_ArGNN.pth','.tools/fingering/python/python.exe','.tools/fingering/python/Lib/site-packages/typing_extensions.py','fingering/worker.py','docs/licenses/FINGERING-NOTICES.txt']) {
  const file=join(root,relative);
  await rename(file,file+'.fault');
  try {await assert.rejects(checkFingeringPackage(root),/ENOENT/);} finally {await rename(file+'.fault',file);}
 }
 return {corruptionRejected:true,missingFilesRejected:6};
}
