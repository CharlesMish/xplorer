// End-to-end smoke-runner regression using Chromium plus a local HTTP fixture.
// The fixture enforces AgentMayUseTab's owner/agent match from agent_gateway.cc.
// It is not the native Xplor gateway; a live Xplor run is still required.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawn} = require('node:child_process');
async function run(scenario) {
 const browser=await chromium.launch({headless:true,
  ...(process.env.CHROMIUM_EXECUTABLE ? {executablePath:process.env.CHROMIUM_EXECUTABLE} : {})});
 const page=await browser.newPage();

 const home=fs.mkdtempSync(path.join(os.tmpdir(),'xplor-runner-'));
 let owner='', count=0, denied=0, identifiedActions=0;
 const server=http.createServer(async(req,res)=>{
  let cdp;
  try {
   if(req.headers.authorization!=='Bearer fixture-token'){res.writeHead(401);res.end('{}');return;}
   let raw='';for await(const x of req)raw+=x;const data=raw?JSON.parse(raw):{};
   const agent=req.headers['x-agent-id'];
   if(req.url.startsWith('/tabs/1:0/') && agent){
    identifiedActions++;
    if(agent!==owner){
     denied++;
     res.setHeader('Content-Type','application/json');
     res.end(JSON.stringify({error:'tab is owned by another agent'}));
     return;
    }
   }
   // HTTP gateway sessions are short-lived. Use raw CDP clicks rather than
   // Playwright's auto-waiting click, which masked native focus assumptions.
   cdp=await page.context().newCDPSession(page);
   await cdp.send('Page.enable');
   async function click(selector) {
    const r=await cdp.send('Runtime.evaluate',{expression:`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(!e)return null;e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`,returnByValue:true});
    if(!r.result.value)return {error:'selector not found'};
    // Native Click reports success without waiting for mouse event acks.
    for(const type of ['mousePressed','mouseReleased'])cdp.send('Input.dispatchMouseEvent',{type,...r.result.value,button:'left',clickCount:1}).catch(()=>{});
    return {clicked:true};
   }
   let out={};
   if(req.url==='/tabs'&&req.method==='GET') out={tabs:owner?[{id:'1:0',owner,url:page.url()}]:[]};
   else if(req.url==='/tabs'&&req.method==='POST'){owner=data.owner;count++;await page.goto(data.url);if(scenario==='suppressed focus')await page.evaluate(()=>document.addEventListener('mousedown',e=>e.preventDefault()));out={ok:true,owner};}
   else if(req.url==='/tabs/1:0/eval')out=await cdp.send('Runtime.evaluate',{expression:data.expression,returnByValue:true,awaitPromise:true});
   else if(req.url==='/tabs/1:0/click'){out=scenario==='dropped standalone click'?{clicked:true}:await click(data.selector);}
   else if(req.url==='/tabs/1:0/type'){out=await click(data.selector);if(!out.error)out=await cdp.send('Input.insertText',{text:data.text});}
   else throw Error('Unexpected route '+req.url);
   res.setHeader('Content-Type','application/json');res.end(JSON.stringify(out));
  }catch(e){res.writeHead(500);res.end(JSON.stringify({error:String(e)}));}
  finally{if(cdp)await cdp.detach();}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 fs.mkdirSync(path.join(home,'.xplorer'));
 fs.writeFileSync(path.join(home,'.xplorer/gateway.json'),JSON.stringify({url:`http://127.0.0.1:${server.address().port}`,token:'fixture-token'}));
 try {
  const env={...process.env,HOME:home,XPLORER_AGENT_ID:'preexisting-agent'};delete env.XPLORER_TOKEN;
  console.log(`Runner scenario: ${scenario}`);
  const p=spawn(process.env.PYTHON || 'python3',['sdk/text_fields_smoke.py'],{
   cwd:path.resolve(__dirname,'../..'),env,stdio:'inherit'});
  const exitCode=await new Promise((resolve,reject)=>{p.on('exit',resolve);p.on('error',reject);});
  assert.equal(exitCode,0,'All ten smoke checks must pass');
  assert.equal(count,1,'Expected exactly one fixture tab');
  assert.ok(identifiedActions>0,'MCP requests must carry an agent identity');
  assert.equal(denied,0,'Runner must use its fixture tab owner as the MCP identity');
  console.log('PASS runner: matching MCP identity accepted by ownership-enforcing fixture (not Xplor).');
 }finally{await new Promise(resolve=>server.close(resolve));await browser.close();fs.rmSync(home,{recursive:true,force:true});}
}
(async()=>{
 await run('normal click focus');
 // Reproduce stale focus without needing native macOS: mousedown's default
 // focus action is suppressed, while click handlers (including the wrapper's
 // explicit focus delegation) still run. This reproduces the reported 6/10
 // pattern with the previous MCP helper; it is not a claim about its Mac cause.
 await run('suppressed focus');
 // A click response is not proof that its events or focus delegation ran.
 // Leave focus on the previous field, reproducing the native 9/10 failure.
 await run('dropped standalone click');
})().catch(e=>{console.error(e);process.exitCode=1;});
