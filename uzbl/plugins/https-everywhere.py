""" HTTPS everywhere support"""

# Prefer the C-based element tree.
try:
    import xml.etree.cElementTree as ET
except:
    import xml.etree.ElementTree as ET
import re
import os
from urllib.parse import urlparse

from .on_set import OnSetPlugin
from .cookies import add_cookie_matcher, Cookies
from uzbl.ext import GlobalPlugin, PerInstancePlugin

xdg_data_home = os.environ.get('XDG_DATA_HOME', os.path.join(os.environ['HOME'], '.local/share'))
RulesetFile = os.path.join(xdg_data_home, 'uzbl/https-everywhere.ruleset')

JSRegExpConverter = re.compile('\\$([1-9])')

class HTTPSEverywhereRulesets(GlobalPlugin):

    def __init__(self, event_manager):
        super(HTTPSEverywhereRulesets, self).__init__(event_manager)

        self.rulesets = []

        self.load_rulesets()

    def load_rulesets(self):
        if not os.path.exists(RulesetFile):
            return

        xml = ET.parse(RulesetFile)
        root = xml.getroot()

        rulesets = []

        for child in root:
            rulesets.append(self.parse_ruleset(child))

        self.rulesets = rulesets

    def convert_js_regex(self, repl):
        return JSRegExpConverter.sub('\\\\g<\\1>', repl)

    def parse_ruleset(self, elem):
        attribs = dict(elem.items())

        ruleset = {
            'name': attribs['name'],
            'flags': [],
            'hosts': [],
            'exclusions': [],
            'rules': [],
            'cookies': []
        }

        if 'default_off' in attribs:
            flags = attribs['default_off'].split(',')
            ruleset['flags'] = set([cond.strip().lower() for cond in flags])

        for child in elem:
            d = dict(child.items())

            if child.tag == 'target':
                ruleset['hosts'].append(d)
            elif child.tag == 'exclusion':
                d['pattern'] = re.compile(d['pattern'])
                ruleset['exclusions'].append(d)
            elif child.tag == 'securecookie':
                ruleset['cookies'].append(d)
            elif child.tag == 'rule':
                # Compile the regex.
                d['from'] = re.compile(d['from'])
                # Replace JS-style back-references with safe Python references.
                d['to'] = self.convert_js_regex(d['to'])

                ruleset['rules'].append(d)

        return ruleset


class HTTPSEverywhere(PerInstancePlugin):

    def __init__(self, uzbl):
        super(HTTPSEverywhere, self).__init__(uzbl)

        self.enabled = 0
        self.allow_downgrades = 0
        self.loaded = False
        self.mask = set()

        uzbl.connect('HTTPS_EVERYWHERE_RELOAD_RULES', self.reload_rules)
        uzbl.connect('HTTPS_EVERYWHERE_RELOAD', self.reload_)
        uzbl.connect('LOAD_START', self.redirect)
        OnSetPlugin[uzbl].on_set('enable_https_everywhere',
                lambda uzbl, k, v: self.set_enabled(v))
        OnSetPlugin[uzbl].on_set('allow_https_everywhere_downgrades',
                lambda uzbl, k, v: self.set_allow_downgrades(v))
        OnSetPlugin[uzbl].on_set('https_everywhere_mask',
                lambda uzbl, k, v: self.set_mask(v))

        self.reload_()

    def reload_rules(self, arg):
        HTTPSEverywhereRulesets[self.uzbl].load_rulesets()

    def reload_(self, arg=None):
        if not self.enabled:
            return

        rulesets = HTTPSEverywhereRulesets[self.uzbl].rulesets
        # Clear all existing cookie rules.
        self.uzbl.send('event CLEAR_SECURE_COOKIES https-everywhere-reload')
        cookies = Cookies[self.uzbl]
        for ruleset in rulesets:
            # Ignore rulesets with masked tags
            if self.mask.intersection(ruleset['flags']):
                continue
            for cookie in ruleset['cookies']:
                # FIXME: This is really slow (~3-4s delay on startup per instance).
                #self.uzbl.send('event SECURE_COOKIE domain \'%s\' name \'%s\'' %
                #        (cookie['host'], cookie['name']))
                add_cookie_matcher(cookies.secure,
                        ['domain', cookie['host'],
                         'name', cookie['name']])

        self.loaded = True

    def redirect(self, uri):
        new_uri = self.apply_rules(uri)
        if new_uri is not None:
            self.uzbl.send('uri %s' % new_uri.replace('@', '\\@'))

    def apply_rules(self, full_uri):
        if not self.enabled:
            return

        full_uri = full_uri.strip("'")

        if '/' not in full_uri:
            full_uri = 'http://%s/' % full_uri
            uri = urlparse(full_uri)
        else:
            uri = urlparse(full_uri)

        rulesets = HTTPSEverywhereRulesets[self.uzbl].rulesets
        for ruleset in rulesets:
            # Ignore rulesets with masked tags.
            if self.mask.intersection(ruleset['flags']):
                continue
            # Ignore the URI if excluded from the ruleset.
            for exclusion in ruleset['exclusions']:
                if exclusion['pattern'].match(full_uri):
                    continue
            apply_rules = False
            # Check if the hosts match.
            for host in ruleset['hosts']:
                hostname = host['host']
                if hostname[0] == '*':
                    # Match subdomains
                    if uri.hostname.endswith(hostname[1:]):
                        apply_rules = True
                        break
                elif hostname == uri.hostname:
                    apply_rules = True
                    break
            # If the hosts failed to match, bail.
            if not apply_rules:
                continue
            for rule in ruleset['rules']:
                if 'downgrade' in rule and not self.allow_downgrades:
                    continue
                new = rule['from'].sub(rule['to'], full_uri)
                # We've made a successful match; make the redirect.
                if not new == full_uri:
                    # Replace '@' in the URI to avoid variable expansion.
                    self.logger.info('Rewriting %r into %r', full_uri, new)
                    return new

    def set_enabled(self, val):
        try:
            self.enabled = bool(int(val))
            if self.enabled and not self.loaded:
                self.reload_()
        except:
            pass

    def set_allow_downgrades(self, val):
        try:
            self.allow_downgrades = bool(int(val))
        except:
            pass

    def set_mask(self, val):
        vals = val.split(',')
        self.mask = set([v.split().lower() for v in vals])
