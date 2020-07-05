const Api = require('./Api');

const fs = require('fs').promises;
const test = require('ava');

let api;

async function getApi() {
    if (!api) {
        const sockPath = await fs.readFile('socket.txt', {
            encoding: 'utf-8'
        });

        api = new Api(sockPath.trim());
    }

    return api;
}

test.beforeEach(async t => {
    t.context = {
        api: await getApi()
    };
});

test('echo', async t => {
    const message = 'Hello!';
    const reply = await t.context.api.echo(message);

    t.is(message, reply);
});
