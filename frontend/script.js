var DEBUG = false;

var globalState = {
    'connectionAttempts': 0,
    'images': {}
};

/**************************** RENDERING ***************************************/

function getTileCoords(i, header) {
    var x = (i % 34)*15 - (header.x_ofs - 15);
    var y = Math.floor(i / 34)*15 - (header.y_ofs - 15);
    return [x,y]
}

function handleButtonsAndClock(header) {
    document.getElementById('clock').style.visibility = (header.time_left == 1023) ? 'hidden' : 'visible';

    document.getElementById('time_left').innerHTML = header.time_left.toString();;
    document.getElementById('button_start').style.visibility = header.button_start ? 'visible' : 'hidden';
    document.getElementById('button_end').style.visibility = header.button_end ? 'visible' : 'hidden';
}

function render(map) {
    if (map == null) {
        map = globalState['map']
    } else {
        globalState['map'] = map;
    }

    var bankStr = globalState['bank'];

    var canvas = document.getElementById('canvas');
    var context = canvas.getContext('2d');

    var w = canvas.width = 495;
    var h = canvas.height = 495;

    context.fillStyle = 'black';
    context.font = "30px monospace"
    context.strokeStyle = 'white';
    context.fillRect(0, 0, w, h);

    if (isConnectionClosed() || map == null) {
        context.strokeText("...", w/2-30, h/2-15);
        handleButtonsAndClock({
            'time_left': 1023,
            'button_end': false,
            'button_start': false
        })
        return;
    }

    handleButtonsAndClock(map.header);

    // draw tiles
    for (var i in map.tiles) {
        var coords = getTileCoords(i, map.header);
        try {
            context.drawImage(globalState.images[bankStr][map.tiles[i]], coords[0], coords[1])
        } catch (e) {
            var missing = document.getElementById("missing");
            context.drawImage(missing, coords[0], coords[1]);
        }
    }

    // draw floating tiles
    for (var i in map.floating_tiles) {
        var x = map.floating_tiles[i].x + 7 - map.header.x_ofs;
        var y = map.floating_tiles[i].y + 7 - map.header.y_ofs;
        var image = globalState.images[bankStr][map.floating_tiles[i].sprite];
        var width = image.width;
        var height = image.height;
        var angle = map.floating_tiles[i].rotation * Math.PI / 180;

        context.translate(x, y);
        context.rotate(-angle);
        try {
            context.drawImage(image, -width / 2, -height / 2, width, height);
        } catch (e) {
            var missing = document.getElementById("missing");
            context.drawImage(missing, -width / 2, -height / 2, width, height);
        }
        context.rotate(angle);
        context.translate(-x, -y);
    }
}

function loadSprites(bank) {
    var bankStr = ('0' + bank.toString());
    bankStr = bankStr.substr(bankStr.length - 2);

    globalState['bank'] = bankStr;

    if (bankStr in globalState['images'])
        return;

    globalState.images[bankStr] = []
    for (var i = 0; i <= 0x1f; i++) {
        var img = new Image()
        var id = ('0' + i.toString());
        id = id.substr(id.length - 2);
        // img.src = 'http://protab./static/img/2017/' + bankStr + '/' + id + '.png';
        img.src = 'img/' + bankStr + '/' + id + '.png';
        var counter = 0;
        img.onload = img.onerror = function() {
            counter++;
            if (counter == 32) {
                render(null);
            }
        }
        globalState.images[bankStr][i] = img;
    }
}

/*************************** CONNECTION MANAGEMENT ****************************/

function setConnectionStatusMsg(msg) {
  document.getElementById('connection_status_msg').innerHTML = msg;
}

function init() {
    var socket = new WebSocket(url);
    socket.binaryType = "arraybuffer";

    socket.onopen = function() {
        console.log('Connection opened...');
        setConnectionStatusMsg('Connected...')
        globalState.connectionAttempts = 0;
        render(null);
    };

    socket.onmessage = function(event){
        if (!event.data instanceof ArrayBuffer) {
            console.error('Received data is not of ArrayBuffer type');
            return;
        }

        if(DEBUG) {
            console.log('Received data:')
            console.log(event.data);
        }

        var data = new Uint8Array(event.data)
    
        // HEADER
        var header = {
            y_ofs: (data[0] & 0xf0 >>> 4) + 15,
            x_ofs: (data[0] & 0x0f) + 15,
            button_start: (data[1] & 0x01) > 0,
            button_end: (data[1] & 0x02) > 0,
            time_left: data[2] + ((data[1] & 0xc0) >>> 6) * 256
        }
        var bank = data[3];

        if(DEBUG) {
            console.log('Header:');
            console.log(header);
        }

        // TILES

        var tiles = []
        var bi = 4;

        var t = 0;
        var repeat = 1;
        var sprite = 0;
        while (tiles.length != 34*34) {
            t = data[bi++];
            repeat = ((t & 0x80) > 0) ? data[bi++] + 3 : 1;
            sprite = t & 0x1f;

            for (var i = 0; i < repeat; i++) tiles.push(sprite);
        }
        
        if(DEBUG) {
            console.log('Tiles:');
            console.log(tiles);
        }

        // FLOATING TILES
        var floating_tiles = []
        while (bi < data.length) {
            var x = data[bi+1] + 256 * ((data[bi] & 0x40) >>> 6);
            var y = data[bi+2] + 256 * ((data[bi] & 0x80) >>> 7);
            var rot = (data[bi] & 0x20) > 0;
            var rotation = 0;
            sprite = data[bi] & 0x1f;

            if (rot) {
                rotation = data[bi+3]*3;
                bi += 4;
            } else {
                bi += 3;
            }

            floating_tiles.push({
                'x': x,
                'y': y,
                'rotation': rotation,
                'sprite': sprite
            })
        }

        if(DEBUG) {
            console.log('Floating tiles:');
            console.log(floating_tiles);
        }
        
        // finalising data bundle
        var map = {
            'tiles': tiles,
            'floating_tiles': floating_tiles,
            'header': header
        };

        loadSprites(bank);
        render(map);
    };
    socket.onclose = function(event) {
        console.log('Connection closed...');
        setConnectionStatusMsg('Not connected');
        reloadConnectionButtonText();
        globalState['map'] = null;
        render(null);
        setTimeout(reconnect, 200);
    };

    globalState['socket'] = socket;

    setConnectionStatusMsg('Connecting...')
    reloadConnectionButtonText();
}

function buttonPress(id) {
    if (isConnectionClosed()) return;
    var socket = globalState['socket']
    var payload = new ArrayBuffer(1);
    var view = new Uint8Array(payload);
    view[0] = id;
    socket.send(payload);

    switch(id) {
        case 1:
            document.getElementById('button_start').style.visibility = 'hidden';
            break;
        case 2:
            document.getElementById('button_end').style.visibility = 'hidden';
            break;
    }
}

function isConnectionClosed() {
  var socket = globalState['socket'];
  return (socket === null || socket === undefined || socket.readyState === 3)
}

function closeConnection() {
    var socket = globalState['socket'];
    socket.close();
}

function reloadConnectionButtonText() {
    var button = document.getElementById('connection_control');
    if (isConnectionClosed()) {
        button.innerHTML = 'Connect'
    } else {
        button.innerHTML = 'Disconnect'
    }
}

function onConnectionButtonClick() {
    var button = document.getElementById('connection_control');
    var socket = globalState['socket'];

    globalState.connectionAttempts = 42;  // magic number

    if (isConnectionClosed()) {
        setTimeout(init, 50);
    } else {
        closeConnection();
    }
}

function reconnect() {
    globalState.connectionAttempts++;
    if (globalState.connectionAttempts < 3)
        init();
}
