#!/usr/bin/perl -I.

use warnings;
use strict;

use Maze;

my $user = 'test';
my $level = 'zzz';

###############################################################################
#
# Priklad pouzivani objektu Maze pro komunikaci
#

my $maze = Maze->new(
    user => $user,
    level => $level,
    wait => 1,           # 0 pro vypnuti cekani na "Spustit" na webu
    trace => 1,          # 0 pro vypnuti vypisovani komunikace
);

my $w = $maze->width();
my $h = $maze->height();

my @map = split / /, $maze->maze();

while (1) {
    my $x = $maze->x();
    my $y = $maze->y();

    my $above = $maze->at($x, $y - 1);

    $maze->move(UP) or die $@;
}

###############################################################################
#
# Jednoducha varianta: funkce rozhodne, kam se pujde.
#

Maze::run($user, $level, sub {
    my ($ok) = @_;
    return $ok ? UP : undef;
});
