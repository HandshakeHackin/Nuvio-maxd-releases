// Cheerio-select 2 consumes the older token entry point and pseudo extension maps.
// Exact css-select 7 sources are locked; these aliases add no provider host APIs.
export * from 'css-select-modern';
export { default } from 'css-select-modern';
export { _compileUnsafe as _compileToken } from 'css-select-modern';
export { filters } from './node_modules/css-select-modern/dist/pseudo-selectors/filters.js';
export { pseudos } from './node_modules/css-select-modern/dist/pseudo-selectors/pseudos.js';
export { aliases } from './node_modules/css-select-modern/dist/pseudo-selectors/aliases.js';
