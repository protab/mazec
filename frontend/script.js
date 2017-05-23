/**
 *  Tyto hodnoty pod stejnými názvy:
 *      username
 *      socket
 *      map
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

function resizeCanvas() {
    var canvas = document.getElementById('canvas');
    canvas.height = window.innerHeight - 30;
    canvas.width = window.innerWidth;
    canvas.style.width = window.innerWidth.toString()+'px';
    canvas.style.height = (window.innerHeight - 30).toString()+'px';
}
window.addEventListener('load', resizeCanvas);

function render(map) {
    globalState['map'] = map;

    var canvas = document.getElementById('canvas');
    var context = canvas.getContext('2d');

    var w = canvas.width;
    var h = canvas.height;

    if (isConnectionClosed()) {
      context.clearRect(0, 0, w, h);
      context.font = "30px monospace"
      context.strokeText("...", w/2-45, h/2-15);
      return;
    };

    context.fillStyle = '#' + ('00000'+(Math.random()*(1<<24)|0).toString(16)).slice(-6); // random color
    context.fillRect(0, 0, w, h);


    // TODO render map
}

function rerender() {
    var map = globalState['map'];
    render(map);
}

function resizeCanvasAndRerender() {
  resizeCanvas();
  rerender();
}
window.addEventListener('resize', resizeCanvasAndRerender);



/*************************** CONNECTION MANAGEMENT ****************************/

function setConnectionStatusMsg(msg) {
  document.getElementById('connection_status_msg').innerHTML = msg;
}

function init() {
    var socket = new WebSocket('ws://protab:1234/' + getUsername());

    socket.onopen = function() {
        console.log('Connection opened...');
        setConnectionStatusMsg('Spojení navázáno...')
        // TODO init
    };

    socket.onmessage = function(event){
        var data = event.data;
        console.log(data);

        // TODO process data
        var map = {};

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
