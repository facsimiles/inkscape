// SPDX-License-Identifier: GPL-2.0-or-later

'use strict';

const NS = {
    xrd: 'http://docs.oasis-open.org/ns/xri/xrd-1.0',
    roster: 'jabber:iq:roster',
    disco_info: 'http://jabber.org/protocol/disco#info',
    pubsub: 'http://jabber.org/protocol/pubsub',
    avatar_metadata: 'urn:xmpp:avatar:metadata',
    avatar_data: 'urn:xmpp:avatar:data',
    nickname: 'http://jabber.org/protocol/nick',
    caps: 'http://jabber.org/protocol/caps',
    ecaps2: 'urn:xmpp:caps',
    hashes: 'urn:xmpp:hashes:2',
    xhtml: 'http://www.w3.org/1999/xhtml',
    svg: 'http://www.w3.org/2000/svg',
    jingle: 'urn:xmpp:jingle:1',
    jingle_sxe: 'urn:xmpp:jingle:transports:sxe',
};

function nsResolver(prefix) {
    return NS[prefix] || null;
}

function parseXPath(elem, xpath, result)
{
    if (result === undefined)
        result = XPathResult.FIRST_ORDERED_NODE_TYPE;
    const value = elem.getRootNode().evaluate(xpath, elem, nsResolver, result, null);
    if (result == XPathResult.FIRST_ORDERED_NODE_TYPE)
        return value.singleNodeValue;
    return value;
}

function parseXPathText(elem, xpath)
{
    const value = parseXPath(elem, xpath);
    if (value === null)
        return null;
    return value.textContent;
}

function displaySpinner(spinner) {
    if ('timeoutid' in spinner.dataset)
        clearTimeout(spinner.dataset.timeoutid);
    spinner.src = 'spinner.svg';
    spinner.title = '';
    spinner.hidden = false;
}

function spinnerOk(spinner) {
    if ('timeoutid' in spinner.dataset)
        clearTimeout(spinner.dataset.timeoutid);
    spinner.src = 'ok.svg';
    spinner.title = '';
    spinner.hidden = false;
    spinner.dataset.timeoutid = setTimeout(function () {
        spinner.hidden = true;
    }, 1000);
}

function spinnerError(spinner, title) {
    if ('timeoutid' in spinner.dataset)
        clearTimeout(spinner.dataset.timeoutid);
    spinner.src = 'error.svg';
    spinner.title = title ? title : '';
    spinner.hidden = false;
}

function hideSpinner(spinner) {
    if ('timeoutid' in spinner.dataset)
        clearTimeout(spinner.dataset.timeoutid);
    spinner.hidden = true;
}

document.addEventListener('DOMContentLoaded', function () {
    let connection = null;

    const jid_element = document.getElementById('jid');
    const pass_element = document.getElementById('pass');
    const connect_button = document.getElementById('connect');
    const spinner_img = document.getElementById('connect-spinner');

    const connected_div = document.getElementById('connected');
    const roster_table = document.getElementById('roster');

    const avatar_img = document.getElementById('avatar');

    function rawInput(data)
    {
        console.log('RECV', data);
    }

    function rawOutput(data)
    {
        console.log('SENT', data);
    }

    connect_button.addEventListener('click', function (evt) {
        if (!connect_button.classList.contains('disconnect')) {
            const jid = jid_element.value;
            getBOSHService(jid).then((bosh_service) => {
                connection = new Strophe.Connection(bosh_service);
                connection.rawInput = rawInput;
                connection.rawOutput = rawOutput;
                connection.connect(jid,
                                   pass_element.value,
                                   onConnect);
            });
        } else if (connection != null) {
            connection.disconnect();
        }
        evt.preventDefault();
    });

    function getBOSHService(jid)
    {
        return new Promise((resolve, reject) => {
            const [nodepart, domainpart] = jid.split('@', 2);
            const url = 'https://' + domainpart + '/.well-known/host-meta';
            const xhr = new XMLHttpRequest();
            xhr.onload = function (evt) {
                const xml = evt.target.responseXML;
                const links = parseXPath(xml, './xrd:XRD/xrd:Link', XPathResult.ORDERED_NODE_ITERATOR_TYPE);
                let bosh_service = null;
                while (true) {
                    const link = links.iterateNext();
                    if (!link)
                        break;
                    if (link.getAttributeNS(null, 'rel') == 'urn:xmpp:alt-connections:xbosh') {
                        bosh_service = link.getAttributeNS(null, 'href');
                        break;
                    }
                    // TODO: also support WebSocket.
                }
                console.log('bosh_service', bosh_service);
                resolve(bosh_service);
            };
            xhr.open('GET', url);
            xhr.send();
        });
    }

    function onConnect(status)
    {
        if (status == Strophe.Status.CONNECTING) {
            console.log('Strophe is connecting.');
            connect_button.value = 'Log out';
            connect_button.classList.add('disconnect');
            jid_element.disabled = true;
            pass_element.disabled = true;
            displaySpinner(spinner_img);
        } else if (status == Strophe.Status.CONNFAIL) {
            console.log('Strophe failed to connect.');
            onDisconnected();
        } else if (status == Strophe.Status.DISCONNECTING) {
            console.log('Strophe is disconnecting.');
            displaySpinner(spinner_img);
        } else if (status == Strophe.Status.DISCONNECTED) {
            console.log('Strophe is disconnected.');
            onDisconnected();
        } else if (status == Strophe.Status.CONNECTED) {
            console.log('Strophe is connected.');
            onConnected();
        }
    }

    function onConnected()
    {
        jid_element.hidden = true;
        pass_element.hidden = true;
        connected_div.hidden = false;
        hideSpinner(spinner_img);
        initRoster(connection);
    }

    function onDisconnected()
    {
        connect_button.value = 'Log in';
        connect_button.classList.remove('disconnect');
        jid_element.hidden = false;
        jid_element.disabled = false;
        pass_element.hidden = false;
        pass_element.disabled = false;
        hideSpinner(spinner_img);
        connected_div.hidden = true;
    }

    function initRoster(connection)
    {
        const resources_dict = {};
        const tr_dict = {};
        const [bare, resource] = connection.jid.split('/', 2);
        addContact(bare);

        function onJingle(result_iq)
        {
            // TODO: don’t assume this is a session-accept.
            console.log('Assuming session-accept:', result_iq);
            const jid = result_iq.getAttributeNS(null, 'from');
            const [bare, resource] = jid.split('/', 2);
            const message = $msg({to: jid})
                .c('sxe', {xmlns: 'urn:xmpp:sxe:0', id: 'what-should-this-be?', session: 'foo'})
                    .c('connect');
            connection.send(message);
        }

        function onJingleError(string)
        {
            console.log('Failed to initiate Jingle session: ' + string);
        }

        function addContact(jid)
        {
            const tr = document.createElementNS(NS.xhtml, 'tr');
            tr.setAttributeNS(null, 'style', 'opacity: 50%');
            resources_dict[jid] = {};
            console.log(resources_dict);
            tr_dict[jid] = tr;
            const td = document.createElementNS(NS.xhtml, 'td');
            const a = document.createElementNS(NS.xhtml, 'a');
            a.setAttributeNS(null, 'href', 'xmpp:' + jid);
            a.addEventListener('click', function(evt) {
                evt.preventDefault();
                const bare = evt.target.href.substr(5);
                console.log(bare);
                const resources = resources_dict[bare];
                let good_resource = null;
                for (let resource in resources) {
                    if (resources[resource]) {
                        good_resource = resource;
                        break;
                    }
                }
                if (good_resource === null)
                    return;
                const jid = bare + '/' + good_resource;
                const iq = $iq({type: 'set', to: jid})
                    .c('jingle', {xmlns: NS.jingle, action: 'session-initiate', initiator: connection.jid, sid: 'foo'})
                        .c('content', {creator: 'initiator', name: 'collaborative edition'})
                            // XXX: Standardise the application namespaces!
                            .c('description', {xmlns: 'urn:xmpp:jingle:apps:svg'}).up()
                            .c('transport', {xmlns: NS.jingle_sxe})
                                .c('host')
                                    .t(jid)
                connection.sendIQ(iq, onJingle, onJingleError.bind(null, 'Jingle session-initiate failed.'));
            });
            const text = document.createTextNode(jid);
            a.appendChild(text);
            td.appendChild(a);
            tr.appendChild(td);
            roster_table.lastChild.appendChild(tr);
        }

        function onRoster(result_iq)
        {
            const items = parseXPath(result_iq, './roster:query/roster:item', XPathResult.ORDERED_NODE_ITERATOR_TYPE);
            while (true) {
                const item = items.iterateNext();
                if (!item)
                    break;
                const jid = item.getAttributeNS(null, 'jid');
                const subscription = item.getAttributeNS(null, 'subscription');
                const name = item.getAttributeNS(null, 'name');
                const groups = item.children;
                console.log("got contact:", jid, subscription, name, groups);
                addContact(jid);
            }
        }

        function onRosterError(string)
        {
            console.log('Failed to retrieve your contact list: ' + string);
        }

        function onPresence(presence)
        {
            function onDiscoInfo(result_iq)
            {
                const features = parseXPath(result_iq, './disco_info:query/disco_info:feature', XPathResult.ORDERED_NODE_ITERATOR_TYPE);
                let has_jingle_transport = false;
                let has_sxe = false;
                while (true) {
                    const feature = features.iterateNext();
                    if (!feature)
                        break;
                    const var_ = feature.getAttributeNS(null, 'var');
                    if (var_ === 'urn:xmpp:jingle:transports:sxe')
                        has_jingle_transport = true;
                    else if (var_ === 'urn:xmpp:sxe:0')
                        has_sxe = true;
                }
                if (has_jingle_transport && has_sxe) {
                    console.log('Hello Inkscape!', jid);
                    resources_dict[bare][resource] = true;
                    console.log(resources_dict);
                }
            }

            function onDiscoInfoError(string)
            {
                console.log('Failed to retrieve contact disco#info: ' + string);
            }

            const jid = presence.getAttributeNS(null, 'from');
            const [bare, resource] = jid.split('/', 2);
            if (resource !== undefined) {
                resources_dict[bare][resource] = false;
                console.log(resources_dict);
            }
            const type = presence.getAttributeNS(null, 'type');
            console.log("got presence:", bare, resource, type);

            // TODO: handle our own presence differently.
            // TODO: handle more than one resource per contact.
            const tr = tr_dict[bare];
            if (type === null) {
                if (tr !== undefined) {
                    tr.setAttributeNS(null, 'style', 'opacity: 100%');
                }
                const caps = parseXPath(presence, './caps:c', XPathResult.ORDERED_NODE_ITERATOR_TYPE).iterateNext();
                if (caps !== null) {
                    const node = caps.getAttributeNS(null, 'node');
                    const ver = caps.getAttributeNS(null, 'ver');
                    if (node !== null && ver !== null) {
                        const iq = $iq({type: 'get', to: jid})
                            .c('query', {xmlns: NS.disco_info, node: node + '#' + ver});
                        connection.sendIQ(iq, onDiscoInfo, onDiscoInfoError.bind(null, 'disco#info query failed.'));
                    }
                }
                // TODO: move to only ecaps2.
                /*
                const hashes = parseXPath(presence, './ecaps2:c/hashes:hash', XPathResult.ORDERED_NODE_ITERATOR_TYPE);
                while (true) {
                    const hash = hashes.iterateNext();
                    if (hash === null)
                        break;
                    console.log(hash);
                }
                */
            } else if (type === 'unavailable') {
                if (tr !== undefined) {
                    tr.setAttributeNS(null, 'style', 'opacity: 50%');
                }
            }

            return true;
        }

        const iq = $iq({type: 'get'})
            .c('query', {xmlns: NS.roster});
        connection.sendIQ(iq, onRoster, onRosterError.bind(null, 'roster query failed.'));

        const presence = $pres();
        connection.addHandler(onPresence, null, 'presence');
        connection.send(presence);
    }
});
