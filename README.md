# RCON
A Source RCON Protocol command-line utility.

## Description

RCON is a POSIX compliant command-line utility to facilitate communication with servers adhering to the Source RCON Protocol defined at the [Valve Developer Community](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol).

## Installation

### Build

The GNU C development libraries and header files are required to build RCON.

* apt: `sudo apt install libc6-dev`

* yum: `sudo yum install glibc-devel`

* zypper: `sudo zypper in linux-glibc-devel`

### Install

`make install` will compile the utility using gcc and copy it to $HOME/bin.  A sample configuration file, rcon.conf, will be copied to $HOME/.config/rcon.  The configuration file should be updated with information regarding your server(s) prior to using RCON.

## Configuration

RCON relies on the existance of the configuration file $HOME/.config/rcon/rcon.conf to obtain server addresses and, optionally, RCON passwords.  A server entry in the configuration file takes the form:

> alias,IP Address,port\[,password\]

The password field is optional, and the user is prompted for the server's password at runtime if one is not provided in the configuration file.

## Usage

`rcon <target> <command>`

**target** is the alias of the server to contact and **command** is the message to send.
