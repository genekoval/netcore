const Api = require('./Api');

const os = require('os');
const path = require('path');
const { exit } = require('process');

const args = process.argv.slice(2);

if (!args.length) {
    console.error('missing program name');
    exit(1);
}

const sockPath = path.join(
    os.tmpdir(),
    args[0]
) + '.sock';

console.log('Connecting to: ', sockPath);

const api = new Api(sockPath);

api.echo('Hello!').then(reply => {
    console.log('Echo: ', reply);
});

api.pid().then(pid => {
    console.log('Server PID: ', pid);
    process.kill(pid);
});
