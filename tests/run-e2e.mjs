import {spawn} from 'node:child_process';
import {resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
const env={...process.env,PATH:join(root,'.tools/llvm-mingw-20261006-ucrt-x86_64/bin')+';'+process.env.PATH};
for(const file of ['service','browser','keyboard-notation','staff-practice','play-along','play-along-drag','ear-flash','final-notation','dense-score','focus-mode']) {
 console.log(`\nNative/browser integration: ${file}`);
 const child=spawn(process.execPath,['--disable-warning=ExperimentalWarning',resolve(root,`tests/${file}.mjs`)],{cwd:root,env,stdio:'inherit'});
 const result=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',code=>resolve(code));});
 if(result!==0)process.exit(result ?? 1);
}
