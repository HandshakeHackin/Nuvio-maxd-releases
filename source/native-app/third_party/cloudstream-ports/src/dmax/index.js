import { provider } from '../../shared/http.js';
import { aurora } from './aurora.js';

const getStreams = aurora({ name: 'DMAX', mainUrl: 'https://dmax.de', serviceIdentifier: 'dmaxde', mediathekSlug: 'sendungen', apiTokenRealm: 'de' });

module.exports = provider(getStreams);
