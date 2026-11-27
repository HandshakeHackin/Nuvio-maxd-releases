import { provider } from '../../shared/http.js';
import { aurora } from '../dmax/aurora.js';

const getStreams = aurora({ name: 'TLC', mainUrl: 'https://tlc.de', serviceIdentifier: 'tlcde', mediathekSlug: 'sendungen', apiTokenRealm: 'de' });

module.exports = provider(getStreams);
