# RCON
A Source RCON Protocol command-line utility.

## Description

RCON is a POSIX compliant command-line utility to facilitate communication with servers adhering to the Source RCON Protocol defined at the [Valve Developer Community](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol).

## Installation

`make install` will compile the utility using gcc and copy it to $HOME/bin.  A sample configuration file, .rcon, will be copied to the current user's home directory.  The configuration file should be updated with information regarding your server(s) before using RCON.

## Configuration

RCON relies on the existance of the configuration file $HOME/.rcon to obtain server addresses and, optionally, RCON passwords.  A server entry in the configuration file takes the form:

> alias,IP Address,port\[,password\]

The password field is optional, and the user is prompted for the server's password at runtime if one is not provided in the configuration file.

## Usage

`rcon <target> <command>`

**target** is the alias of the server to contact and **command** is the message to send.
