/**
 *  Tyto hodnoty pod stejnými názvy:
 *      username
 *      socket
 */
var globalState = {};

function getUsername() {
    var username = globalState['username'];
    if (username === null || username === undefined) {
        var oldUsername = localStorage.getItem("username");
        username = prompt("Zadej své uživatelské jméno:", (oldUsername === undefined ? 'jmeno' : oldUsername));
        localStorage.setItem("username", username);
        globalState['username'] = username;
    }

    return username;
}

window.addEventListener('resize', function resizeCanvas() {
    var canvas = document.getElementById('canvas');
    canvas.height = window.innerHeight;
    canvas.width = window.innerWidth;

    render(null);
});

function render(map) {
    var canvas = document.getElementById('canvas');
    var context = canvas.getContext('2d');

    var w = canvas.width;
    var h = canvas.height;

    context.fillStyle = '#' + ('00000'+(Math.random()*(1<<24)|0).toString(16)).slice(-6); // random color
    context.fillRect(0, 0, w, h);


    // TODO render map
}

function init() {
    var socket = new WebSocket('ws://protab:1234/' + getUsername());

    socket.onopen = function() {
        console.log('Connection opened...');
        // TODO init
    };

    socket.onmessage = function(event){
        var data = event.data;
        console.log(data);

        // TODO process data
        map = {};

        render(map);
    };
    socket.onclose = function(event) {
        render();
        if (!event.wasClean) {
            alert("Spojení se serverem bylo nečekaně uzavřeno nebo se nepodařilo. Po zavření dialogu bude uskutečněn nový pokus o spojení.")
            setTimeout(init, 1000);
            globalState['socket'] = null;
        }
    };

    globalState['socket'] = socket;
}

function close() {
    var socket = globalState['socket'];
    socket.close();
}
