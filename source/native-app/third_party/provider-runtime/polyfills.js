// Internal browser dependency shim, not a host process API.
export const process={env:{NODE_ENV:'production'},browser:true,version:'',versions:{},
 nextTick:(fn,...args)=>Promise.resolve().then(()=>fn(...args))};
