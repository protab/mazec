package Maze;

use warnings;
use strict;

use Carp;
use IO::Socket;

use parent 'Exporter';

our @EXPORT = qw(UP DOWN LEFT RIGHT run);

use constant {
    UP => 'w',
    DOWN => 's',
    LEFT => 'a',
    RIGHT => 'd',
};

# Vytvori nove pripojeni na server.
sub new {
    my ($class, %args) = @_;

    $args{$_} or confess("Maze needs $_")
        for qw(user level);

    $args{wait}  //= 1;  # Má se čekat na spuštění v GUI?
    $args{trace} //= 1;  # Mají se vypisovat příkazy na výstup?

    my $socket = IO::Socket::INET->new(
        PeerHost => $args{host} // 'protab',
        PeerPort => $args{port} // 4000,
        Proto    => 'tcp',
    ) // croak "Chyba pripojeni k serveru: $@";

    my $self = bless {
        socket => $socket,
        %args
    }, $class;

    $self->command_done("USER $args{user}");
    $self->command_done("LEVL $args{level}");
    $self->wait() if $args{wait};

    return $self;
}

# Odešle jeden příkaz a vrátí odpověď jako dvouprvkový seznam, např. ('DATA', '42')
sub command {
    my ($self, $cmd) = @_;

    $cmd =~ tr/\n//d;

    local $SIG{PIPE} = sub {
        croak "Spojení se serverem je již ukončeno.";
    };

    print STDERR "<<< $cmd\n" if $self->{trace};

    $self->{socket}->print("$cmd\n");
    $self->{socket}->flush();

    my $resp = $self->{socket}->getline();
    croak "Server neodpovedel" unless defined $resp;

    print STDERR "\t>>> $resp" if $self->{trace};

    if (my ($code, $data) = ($resp =~ /^([A-Z]{4}) ?(.*)\n/)) {
        return $code, $data;
    }

    croak "Neplatna odpoved serveru: $resp\n";
}

# Odešle příkaz, na který se očekává odpověď DONE nebo NOPE.
# Vrací 1, pokud byla odpoveď DONE, 0 v opačném případě (a nastaví $@).
#
sub command_handle {
    my ($self, $command) = @_;

    my ($code, $data) = $self->command($command);

    if ($code eq 'DONE') {
        return 1;
    } elsif ($code eq 'NOPE') {
        $@ = "$data\n";
        return 0;
    } elsif ($code eq 'OVER') {
        die "$data\n";
    } else {
        croak "Neocekavana odpoved serveru: $code $data";
    }
}

# Odešle příkaz, na který se očekává odpověď DONE.
sub command_done {
    my ($self, $cmd) = @_;
    $self->command_handle($cmd) or die;
}

# Odešle příkaz, na který se očekává odpověď DATA.
# Vrací obsah dat.
sub command_data {
    my ($self, $cmd) = @_;

    my ($code, $data) = $self->command($cmd);
    croak "Neocekavana odpoved serveru: $code $data" unless $code eq 'DATA';

    return $data;
}

# Počká, až klineš na tlačítko "Spustit"
sub wait {
    shift->command_done('WAIT');
}

# Vrátí šířku plochy
sub width {
    shift->command_data('GETW');
}

# Vrátí výšku plochy
sub height {
    shift->command_data('GETH');
}

# Vrátí aktuální pozici X
sub x {
    shift->command_data('GETX');
}

# Vrátí aktuální pozici Y
sub y {
    shift->command_data('GETY');
}

# Vrátí mapu celého bludiště
sub maze {
    shift->command_data('MAZE');
}

# Vrátí, co je na daném políčku
sub at {
    my ($self, $x, $y) = @_;
    return $self->command_data('WHAT', $x, $y);
}

# Pokusí se o pohyb. Vrací zda byl pohyb úspěšný.
sub move {
    my ($self, $dir) = @_;
    return $self->command_handle("MOVE $dir");
}

# Spustí smyčku pro jednoduchý režim. Parametrem je funkce, která vrací další
# pohyb. Jako parametr tato funkce dostane pouze informaci, jestli se poslední pohyb provedl.
sub loop {
    my ($self, $loop) = @_;
    eval {
        my $ok = 1;
        while (my $dir = $loop->($ok)) {
            $ok = $self->move($dir);
        }
    } or do {
        print $@;
    }
}

# Spustí mazec ve zjednodušeném režimu.
# @staticmethod
sub run ($$$@) {
    my ($user, $level, $loop) = @_;

    Maze->new(
        user => $user,
        level => $level,
    )->loop($loop);
}
