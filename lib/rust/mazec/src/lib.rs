use core::fmt::Debug;
use std::io::{BufRead, BufReader};
use std::{io::Write, net::TcpStream};

enum ClientCommand {
    User(String),
    Levl(String),
    Wait,
    GetW,
    GetH,
    GetX,
    GetY,
    What(usize, usize),
    Maze,
    Move(char),
}

#[derive(Debug)]
pub enum ServerError {
    Nope(String),
    Over(String),
}

enum ServerResponse {
    Done,
    Data(Vec<usize>),
    Nope(String),
    Over(String),
}

struct GameState {
    width: usize,
    height: usize,
}

impl ClientCommand {
    fn serialize(&self) -> Vec<u8> {
        use ClientCommand::*;
        match self {
            User(s) => format!("USER {s}\n").into_bytes(),
            Levl(s) => format!("LEVL {s}\n").into_bytes(),
            Wait => b"WAIT\n".to_vec(),
            GetW => b"GETW\n".to_vec(),
            GetH => b"GETH\n".to_vec(),
            GetX => b"GETX\n".to_vec(),
            GetY => b"GETY\n".to_vec(),
            What(x, y) => format!("WHAT {x} {y}\n").into_bytes(),
            Maze => b"MAZE\n".to_vec(),
            Move(c) => format!("MOVE {c}\n").into_bytes(),
        }
    }
}

impl ServerResponse {
    fn deserialize(line: &str) -> Self {
        use ServerResponse::*;
        let (tag, rest) = line.split_at(4);
        match tag {
            "DONE" => Done,
            "DATA" => Data(
                rest.split_whitespace()
                    .map(|s| s.parse().unwrap())
                    .collect(),
            ),
            "NOPE" => Nope(rest.to_string()),
            "OVER" => Over(rest.to_string()),
            _ => panic!("Unknown response tag: {}", tag),
        }
    }
    fn done_or(&self) -> Result<(), ServerError> {
        use ServerResponse::*;
        match self {
            Done => Ok(()),
            Nope(e) => Err(ServerError::Nope(e.clone())),
            Over(e) => Err(ServerError::Over(e.clone())),
            _ => panic!("Expected DONE, NOPE or OVER"),
        }
    }
    fn get_all_data(&self) -> Result<Vec<usize>, ServerError> {
        use ServerResponse::*;
        match self {
            Data(data) => Ok(data.clone()),
            Nope(e) => Err(ServerError::Nope(e.clone())),
            Over(e) => Err(ServerError::Over(e.clone())),
            Done => panic!("Did not expect DONE response"),
        }
    }
    fn get_data(&self, i: usize) -> Result<usize, ServerError> {
        self.get_all_data().map(|x| x[i])
    }
}

pub enum Move {
    W,
    A,
    S,
    D,
    Other(char),
}
impl From<char> for Move {
    fn from(c: char) -> Self {
        use Move::*;
        match c {
            'w' | 'W' => W,
            'a' | 'A' => A,
            's' | 'S' => S,
            'd' | 'D' => D,
            _ => Other(c.to_ascii_lowercase()),
        }
    }
}
impl Move {
    const fn as_char(&self) -> char {
        use Move::*;
        match self {
            W => 'w',
            A => 'a',
            S => 's',
            D => 'd',
            Other(c) => *c,
        }
    }
}

pub struct Client {
    sock: TcpStream,
    game_state: GameState,
}

impl Client {
    fn communicate(&mut self, command: ClientCommand) -> ServerResponse {
        let buf = command.serialize();
        self.sock.write_all(&buf).unwrap();
        let mut reader = BufReader::new(&self.sock);
        let mut buf = String::new();
        reader.read_line(&mut buf).unwrap();
        ServerResponse::deserialize(&buf)
    }
    #[must_use]
    pub fn new(user: &str, levl: &str, do_wait: bool) -> Self {
        Self::new_raw("i.protab.cz", 4000, user, levl, do_wait)
    }
    #[must_use]
    pub fn new_raw(host: &str, port: u16, user: &str, levl: &str, do_wait: bool) -> Self {
        let mut this = Client {
            sock: TcpStream::connect((host, port)).unwrap(),
            game_state: GameState {
                width: 0,
                height: 0,
            },
        };
        this.communicate(ClientCommand::User(user.to_string()))
            .done_or()
            .expect("command `USER` failed");
        this.communicate(ClientCommand::Levl(levl.to_string()))
            .done_or()
            .expect("command `LEVL` failed");
        if do_wait {
            this.wait().expect("command `WAIT` failed");
        }
        this.game_state.width = this
            .communicate(ClientCommand::GetW)
            .get_data(0)
            .expect("command `GETW` failed");
        this.game_state.height = this
            .communicate(ClientCommand::GetH)
            .get_data(0)
            .expect("command `GETH` failed");
        this
    }
    pub fn get_x(&mut self) -> Result<usize, ServerError> {
        self.communicate(ClientCommand::GetX).get_data(0)
    }
    pub fn get_y(&mut self) -> Result<usize, ServerError> {
        self.communicate(ClientCommand::GetY).get_data(0)
    }
    #[must_use]
    pub const fn get_w(&self) -> usize {
        self.game_state.width
    }
    #[must_use]
    pub const fn get_h(&self) -> usize {
        self.game_state.height
    }
    pub fn what(&mut self, x: usize, y: usize) -> Result<usize, ServerError> {
        self.communicate(ClientCommand::What(x, y)).get_data(0)
    }
    pub fn mov<M: Into<Move>>(&mut self, c: M) -> Result<(), ServerError> {
        self.communicate(ClientCommand::Move(c.into().as_char()))
            .done_or()
    }
    pub fn wait(&mut self) -> Result<(), ServerError> {
        self.communicate(ClientCommand::Wait).done_or()
    }
    pub fn maze(&mut self) -> Result<Vec<usize>, ServerError> {
        self.communicate(ClientCommand::Maze).get_all_data()
    }
    pub fn maze_shaped(&mut self) -> Result<Vec<Vec<usize>>, ServerError> {
        let data = self.communicate(ClientCommand::Maze).get_all_data()?;
        let width = self.game_state.width;
        let height = self.game_state.height;
        let mut maze = vec![vec![0; width]; height];
        for i in 0..height {
            for j in 0..width {
                maze[i][j] = data[i * width + j];
            }
        }
        Ok(maze)
    }
}
