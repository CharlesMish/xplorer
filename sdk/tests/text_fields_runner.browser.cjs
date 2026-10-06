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
(async()=>{
 const browser=await chromium.launch({headless:true,
  ...(process.env.CHROMIUM_EXECUTABLE ? {executablePath:process.env.CHROMIUM_EXECUTABLE} : {})});
 const page=await browser.newPage();
 const cdp=await page.context().newCDPSession(page);
 const home=fs.mkdtempSync(path.join(os.tmpdir(),'xplor-runner-'));
 let owner='', count=0, denied=0, identifiedActions=0;
 const server=http.createServer(async(req,res)=>{
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
   let out={};
   if(req.url==='/tabs'&&req.method==='GET') out={tabs:owner?[{id:'1:0',owner,url:page.url()}]:[]};
   else if(req.url==='/tabs'&&req.method==='POST'){owner=data.owner;count++;await page.goto(data.url);out={ok:true,owner};}
   else if(req.url==='/tabs/1:0/eval')out=await cdp.send('Runtime.evaluate',{expression:data.expression,returnByValue:true,awaitPromise:true});
   else if(req.url==='/tabs/1:0/click'){await page.locator(data.selector).click();out={clicked:true};}
   else if(req.url==='/tabs/1:0/type'){await page.locator(data.selector).click();out=await cdp.send('Input.insertText',{text:data.text});}
   else throw Error('Unexpected route '+req.url);
   res.setHeader('Content-Type','application/json');res.end(JSON.stringify(out));
  }catch(e){res.writeHead(500);res.end(JSON.stringify({error:String(e)}));}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 fs.mkdirSync(path.join(home,'.xplorer'));
 fs.writeFileSync(path.join(home,'.xplorer/gateway.json'),JSON.stringify({url:`http://127.0.0.1:${server.address().port}`,token:'fixture-token'}));
 try {
  const env={...process.env,HOME:home,XPLORER_AGENT_ID:'preexisting-agent'};delete env.XPLORER_TOKEN;
  const p=spawn(process.env.PYTHON || 'python3',['sdk/text_fields_smoke.py'],{
   cwd:path.resolve(__dirname,'../..'),env,stdio:'inherit'});
  process.exitCode=await new Promise(resolve=>p.on('exit',resolve));
  assert.equal(count,1,'Expected exactly one fixture tab');
  assert.ok(identifiedActions>0,'MCP requests must carry an agent identity');
  assert.equal(denied,0,'Runner must use its fixture tab owner as the MCP identity');
  console.log('PASS runner: matching MCP identity accepted by ownership-enforcing fixture (not Xplor).');
 }finally{await new Promise(resolve=>server.close(resolve));await browser.close();fs.rmSync(home,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
