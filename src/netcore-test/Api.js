const Server = require('./Server');

class Api {
    constructor(sockPath) {
        this.sockPath = sockPath;
    }

    connect() {
        const server = new Server(this.sockPath);

        server.on('error', message => {
            console.error('Error: ', message);
        });

        return server;
    }

    async echo(message) {
        return this.connect().get('echo', message);
    }

    async pid() {
        return this.connect().get('pid', '');
    }
}

module.exports = Api;
