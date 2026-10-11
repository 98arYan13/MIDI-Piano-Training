import {existsSync} from 'node:fs';
import {join} from 'node:path';
export function browserPath() {
 const candidates=[process.env.SR_BROWSER,...[process.env['PROGRAMFILES(X86)'],process.env.PROGRAMFILES,process.env.LOCALAPPDATA].filter(Boolean).flatMap(root=>[join(root,'Microsoft/Edge/Application/msedge.exe'),join(root,'Google/Chrome/Application/chrome.exe')])].filter(Boolean);
 const path=candidates.find(existsSync);
 if(!path)throw Error('Install Edge or Chrome, or set SR_BROWSER to its executable. No browser is downloaded by these tests.');
 return path;
}
