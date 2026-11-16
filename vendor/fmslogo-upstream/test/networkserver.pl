#!/usr/bin/perl -W
############################################################
# A simple test application that opens a socket to a given
# host/port, sends some data, then closes the socket.
#
# Usage: networkserver.pl <test> <port>
#
# This is part of network.lgo
#
############################################################

use Socket;
use POSIX qw(_exit);
use strict;

my $test = shift @ARGV or die "not enough parameters";
my $port = shift @ARGV or die "not enough parameters";

my $proto = getprotobyname('tcp');
socket(SERVER, PF_INET, SOCK_STREAM, $proto) or die "socket: $!";
setsockopt(SERVER, SOL_SOCKET, SO_REUSEADDR, pack("l", 1)) or die "setsockopt: $!";

bind(SERVER, sockaddr_in($port, INADDR_ANY)) or die "bind: $!";
listen(SERVER, SOMAXCONN) or die "listen: $!";

my $paddr = accept(CLIENT, SERVER);
my ($remoteport, $iaddr) = sockaddr_in($paddr);

# change the server's behavior, depending on the test we're running
if ($test eq 'simple')
{
  print CLIENT "hello\0";
}
elsif ($test eq 'nonul')
{
  print CLIENT "Hello";
}
elsif ($test eq 'largepacket')
{
  print CLIENT ("abcdefghijklmnopqrstuvwxyz" x 1000), "\0";
}
elsif ($test eq 'echo')
{
  my $data = '';
  defined recv(CLIENT, $data, 1024, 0) or die "recv: $!";
  print CLIENT $data;
}
elsif ($test eq 'ord')
{
  my $data = '';
  defined recv(CLIENT, $data, 1024, 0) or die "recv: $!";
  print CLIENT join ' ', unpack("C*", $data) or die $!;
  print CLIENT "\0" or die $!;
}
elsif ($test eq 'ansi')
{
  # Bytes outside of the ASCII range.
  # How this is interpreted within FMSLogo depends on the system code page.
  # For example, 0x80 in windows-1252 is a Euro sign.
  # In the Latin-1 portion of Unicode, 0x80 is a control character.
  print CLIENT map chr, (0x80..0xFF, 0);
}
elsif ($test eq 'unicode')
{
  # "word" in Greek (logos)
  # lambda - omicron with tonos - gamma - omicron - sigma final
  print CLIENT "\x{03BB}\x{03CC}\x{03B3}\x{03BF}\x{03C2}\0";
}
elsif ($test eq 'fastreset')
{
  # I don't know of any way to directly sent a RST from Perl,
  # but if the process exits abruptly, then the host operating
  # system will do so.
  _exit(0);
}
elsif ($test eq 'slowreset')
{
  # Reads some data then resets.
  my $data = '';
  defined recv(CLIENT, $data, 1024, 0) or die "recv: $!";
  _exit(0);
}

close CLIENT;
close SERVER;
