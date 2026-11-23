// Cheerio's adapter still reads the predicates that DomUtils 4 moved to DomHandler.
export * from 'domutils-modern';
export { isTag, isText, isComment, isCDATA, isDocument, isDirective, hasChildren } from 'domhandler';
