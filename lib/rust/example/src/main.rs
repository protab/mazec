use std::io::Read;
use mazec::Client;

fn main() {
    let mut client = Client::new("koskja", "tardis", false);
    let mut character = [0];
    while let Ok(c) = std::io::stdin().read(&mut character) {
        if c == 0 || character[0] == b'\n' {
            continue;
        }
        if let Err(e) = client.mov(character[0] as char) {
            println!("Error moving {}: {:?}", character[0] as char, e);
        }
    }
}