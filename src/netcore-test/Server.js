const EventEmitter = require('events').EventEmitter;
const net = require('net');

class Server extends EventEmitter {
    constructor(sockPath) {
        super();

        this.listen(sockPath);
    }

    listen(sockPath) {
        this.client = net.connect(sockPath);

        this.client.setEncoding('utf-8');

        this.client.on('data', data => {
            const message = JSON.parse(data);

            this.emit(
                message['event'],
                message['data']
            );
        });
    }

    async get(event, data) {
        return new Promise(resolve => {
            this.once(event, response => resolve(response));
            this.send(event, data);
        });
    }

    send(event, data) {
        this.client.write(JSON.stringify({event, data}));
    }
}

module.exports = Server;
