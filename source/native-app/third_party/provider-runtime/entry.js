// Host API for isolated Nuvio provider scripts. No Node filesystem or process APIs.
const { Buffer } = require('buffer/');
globalThis.Buffer = Buffer;
globalThis.TextEncoder = class { encode(s='') { return new Uint8Array(Buffer.from(String(s),'utf8')); } };
globalThis.TextDecoder = class { decode(b) { return Buffer.from(b || []).toString('utf8'); } };
const { URL, URLSearchParams } = require('whatwg-url');
globalThis.URL=URL; globalThis.URLSearchParams=URLSearchParams;
const cheerio=require('cheerio-without-node-native');
globalThis.crypto={getRandomValues(a){ const b=new Uint8Array(__random(a.byteLength)); new Uint8Array(a.buffer,a.byteOffset,a.byteLength).set(b); return a; }};
const CryptoJS=require('crypto-js');
globalThis.global=globalThis; globalThis.self=globalThis;
globalThis.require=(id)=>{ if(['cheerio','cheerio-without-node-native','react-native-cheerio'].includes(id))return cheerio; if(id==='crypto-js')return CryptoJS; throw new Error('Unsupported provider module: '+id); };
globalThis.atob=s=>Buffer.from(String(s),'base64').toString('latin1');
globalThis.btoa=s=>Buffer.from(String(s),'latin1').toString('base64');
globalThis.console={log(){},warn(){},error(){},debug(){},info(){}};
class Headers {
 constructor(init={}) {this.values={}; if(init instanceof Headers)init=init.values; if(Array.isArray(init))for(const [k,v] of init)this.set(k,v); else for(const k of Object.keys(init))this.set(k,init[k]);}
 set(k,v){this.values[String(k).toLowerCase()]=String(v);} get(k){return this.values[String(k).toLowerCase()]??null;}
 has(k){return this.get(k)!==null;} append(k,v){this.set(k,this.has(k)?this.get(k)+', '+v:v);} delete(k){delete this.values[String(k).toLowerCase()];}
 forEach(fn){for(const k of Object.keys(this.values))fn(this.values[k],k,this);} *entries(){yield* Object.entries(this.values);} [Symbol.iterator](){return this.entries();}
}
globalThis.Headers=Headers;
globalThis.AbortController=class {constructor(){this.signal={aborted:false,addEventListener(){},removeEventListener(){}};}abort(){this.signal.aborted=true;}};
globalThis.fetch=async (input,options={})=>{
 if(options.signal?.aborted)throw new Error('Request aborted');
 const body=options.body instanceof URLSearchParams?options.body.toString():options.body;
 const req={url:String(input),method:String(options.method||'GET').toUpperCase(),headers:new Headers(options.headers).values,body:body==null?'':String(body),redirect:options.redirect||'follow'};
 const r=JSON.parse(__fetch(JSON.stringify(req))); if(r.error)throw new Error(r.error);
 const bytes=Buffer.from(r.body,'base64');
 const response={status:r.status,statusText:'',ok:r.status>=200&&r.status<300,url:r.url,redirected:r.url!==req.url,headers:new Headers(r.headers),bodyUsed:false,
 async text(){this.bodyUsed=true;return bytes.toString('utf8');},async json(){return JSON.parse(await this.text());},async arrayBuffer(){this.bodyUsed=true;return Uint8Array.from(bytes).buffer;},clone(){return {...this};}};
 return response;
};
globalThis.setTimeout=(fn,delay=0,...args)=>__timer(fn,Number(delay)||0,args);
globalThis.clearTimeout=id=>__clearTimer(id);
globalThis.setInterval=()=>{throw new Error('Provider intervals are unsupported');};
globalThis.clearInterval=globalThis.clearTimeout;
globalThis.module={exports:{}};globalThis.exports=module.exports;
