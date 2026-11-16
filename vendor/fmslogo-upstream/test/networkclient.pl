#! /usr/bin/perl -W
############################################################
# A simple test application that opens a socket to a given
# host/port, sends some data, then closes the socket.
#
# Usage: networkclient.pl <test> <hostname> <port>
#
# This is part of network.lgo
#
############################################################

use Socket;
use POSIX qw(_exit);
use strict;

my $test     = shift @ARGV or die "not enough parameters";
my $hostname = shift @ARGV or die "not enough parameters";
my $port     = shift @ARGV or die "not enough parameters";

my $proto = getprotobyname('tcp');
socket(SOCKET, PF_INET, SOCK_STREAM, $proto) or die $!;
my $sin = sockaddr_in($port, inet_aton($hostname));

connect(SOCKET, $sin) or die $!;

if ($test eq 'simple')
{
  # send a simple message.
  print SOCKET "hello\0" or die $!;
}
elsif ($test eq 'nonul')
{
  # send a short message with no framing character.
  print SOCKET "hello" or die $!;
}
elsif ($test eq 'special')
{
  # send special characters.
  print SOCKET "one   two; three\n[a  b]  {1 2}\n\0" or die $!;
}
elsif ($test eq 'delay')
{
  sleep 1;
  print SOCKET "hello\0" or die $!;
  flush SOCKET or die $!;
}
elsif ($test eq 'fastreset')
{
  # I don't know of any way to directly sent a RST from Perl,
  # but if the process exits abruptly, then the host operating
  # system will do so.
  _exit(0);
}
else
{
  die "Unknown test parameter: $test";
}

close (SOCKET) or die $!;
