# RCON

A POSIX-compliant command-line utility for communicating with servers that implement the Source RCON Protocol (as defined on the [Valve Developer Community](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol)).

## Description

RCON lets you send commands to a Source engine dedicated server (or any other server speaking the same protocol) from the command line. Configuration is kept in a simple text file so you do not have to re-type addresses, ports, or passwords for every invocation.

## Installation

### Build requirements

GNU C development libraries and headers are required.

- Debian/Ubuntu: `sudo apt install libc6-dev`
- Fedora/RHEL: `sudo yum install glibc-devel` (or `dnf`)
- openSUSE: `sudo zypper in linux-glibc-devel`

### Build and install

```bash
make
make install

