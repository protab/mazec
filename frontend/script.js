var DEBUG = true;

/**
 *  Tyto hodnoty pod stejnými názvy:
 *      username
 *      socket
 *      map
 *      images
 */
var globalState = {};

function getUsername() {
    var username = globalState['username'];
    if (username === null || username === undefined) {
        var oldUsername = localStorage.getItem("username");
        username = prompt("Zadej své uživatelské jméno:", (oldUsername == null ? 'jmeno' : oldUsername));
        localStorage.setItem("username", username);
        globalState['username'] = username;
    }

    return username;
}

/**************************** RENDERING ***************************************/

function getTileCoords(i, header) {
    var x = (i % 34)*15 + 15 + header.x_ofs;
    var y = Math.floor(i / 34)*15 + 15 + header.y_ofs;
    return [x,y]
}

function handleButtonsAndClock(header) {
    document.getElementById('time_left').innerHTML = header.time_left.toString();
    document.getElementById('button_start').style.display = header.button_start ? 'block' : 'none';
    document.getElementById('button_end').style.display = header.button_start ? 'block' : 'none';
}

function render(map) {
    globalState['map'] = map;

    var canvas = document.getElementById('canvas');
    var context = canvas.getContext('2d');

    var w = canvas.width = 495;
    var h = canvas.height = 495;

    if (isConnectionClosed()) {
      context.clearRect(0, 0, w, h);
      context.font = "30px monospace"
      context.strokeText("...", w/2-30, h/2-15);
      return;
    }

    // draw tiles
    for (var i in map.tiles) {
        var coords = getTileCoords(i);
        context.drawImage(globalState.images[map.tiles[i]], coords[0], coords[1])
    }

    // draw floating tiles
    for (var i in map.floating_tiles) {
        var x = map.floating_tiles[i].x;
        var y = map.floating_tiles[i].y;
        var width = globalState.images[map.floating_tiles[i].sprite].width;
        var height = globalState.images[map.floating_tiles[i].sprite].height;
        var angle = map.floating_tiles[i].rotation * Math.PI / 180;

        context.translate(x, y);
        context.rotate(angle);
        context.drawImage(image, -width / 2, -height / 2, width, height);
        context.rotate(-angle);
        context.translate(-x, -y);
    }
}

function rerender() {
    var map = globalState['map'];
    render(map);
}

function loadAndSaveAllSprites() {
    globalState.images = []
    for (var i = 0; i < 0x1f; i++) {
        var img = new Image()
        var id = ('0' + i.toString());
        id = id.substr(id.length - 2);
        img.src = 'http://protab./static/img/2017/sprite' + id + '.png'
        globalState.images[i] = img;
    }
}
window.addEventListener('load', loadAndSaveAllSprites)

/*************************** CONNECTION MANAGEMENT ****************************/

function setConnectionStatusMsg(msg) {
  document.getElementById('connection_status_msg').innerHTML = msg;
}

function init() {
    var socket = new WebSocket('ws://protab:1234/' + getUsername());
    socket.binaryType = "arraybuffer";

    socket.onopen = function() {
        console.log('Connection opened...');
        setConnectionStatusMsg('Spojení navázáno...')
        // TODO init
    };

    socket.onmessage = function(event){
        if (!data instanceof ArrayBuffer) {
            console.error('Prijata data nejsou typu ArrayBuffer');
            return;
        }

        if(DEBUG) {
            console.log('Prijata data:')
            console.log(event.data);
        }

        var data = new Uint8Array(data)
    
        // HEADER
        var header = {
            y_ofs: (data[0] & 0xf0 >>> 4) + 15,
            x_ofs: (data[0] & 0x0f) + 15,
            button_start: data[1] & 0x01 > 0,
            button_end: data[1] & 0x02 > 0,
            time_left: data[2] + (data[1] & 0xc0 >>> 6) * 2**8
        }

        if(DEBUG) {
            console.log('Header:');
            console.log(header);
        }

        // TILES

        var tiles = []
        var bi = 3;

        var t = 0;
        var repeat = 1;
        var sprite = 0;
        while (tiles.length != 34*34) {
            t = data[bi++];
            repeat = ((t & 0x80) > 0) ? data[bi++] : 1;
            sprite = t & 0x1f;

            for (var i; i < repeat; i++) tiles.push(sprite);
        }
        
        if(DEBUG) {
            console.log('Tiles:');
            console.log(tiles);
        }

        // FLOATING TILES
        var floating_tiles = []
        while (bi < data.length) {
            var x = data[bi+1] + 2**8 * ((data[bi] & 0x80) >>> 7);
            var y = data[bi+2] + 2**8 * ((data[bi] & 0x40) >>> 6);
            var rot = data[bi] & 0x20 > 0;
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

        render(map);
    };
    socket.onclose = function(event) {
        setConnectionStatusMsg('Nepřipojeno');
        reloadConnectionButtonText();
        render();
    };

    globalState['socket'] = socket;


    setConnectionStatusMsg('Navazování spojení...')
    reloadConnectionButtonText();
}

function buttonPress(id) {
    if (isConnectionClosed()) return;
    var socket = globalState['socket']
    var payload = new ArrayBuffer(1);
    var view = new Uint8Array(payload);
    view[0] = id;
    socket.send(payload)
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
        button.innerHTML = 'Připojit'
    } else {
        button.innerHTML = 'Ukončit'
    }
}

function onConnectionButtonClick() {
    var button = document.getElementById('connection_control');
    var socket = globalState['socket'];

    if (isConnectionClosed()) {
        setTimeout(init, 50);
    } else {
        closeConnection();
    }
}
