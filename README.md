# Nex Daemon for Inetd

A simple, lightweight Nex protocol server written in C99, designed to work with inetd/xinetd to serve static files and directories.

## Features

- **C99 Compliant**: Portable POSIX C code
- **Inetd Compatible**: Works seamlessly with inetd/xinetd
- **Secure**: Prevents directory traversal attacks using path canonicalization
- **Directory Listings**: Automatically generates sorted directory listings
- **MIME Type Detection**: Serves files with appropriate MIME types
- **Zero Configuration**: Simple command-line interface
- **Lightweight**: Minimal dependencies and memory footprint

## Building

### Requirements

- GCC or compatible C compiler
- GNU Make
- POSIX-compliant system (Linux, BSD, etc.)

### Compilation

```bash
make
```

This will produce the `nexd` binary.

### Installation

To install system-wide (requires root):

```bash
sudo make install
```

By default, this installs to `/usr/local/bin`. To install to a different location:

```bash
make install PREFIX=/opt/local
```

## Usage

### Standalone Testing

For testing purposes, you can run the server directly:

```bash
echo -e "/file.txt\r" | ./nexd /path/to/directory
```

### With Inetd

1. **Add to `/etc/services`** (if not already present):

```
nex  1900/tcp  # Nex Protocol
```

2. **Configure inetd** in `/etc/inetd.conf`:

```
nex stream tcp nowait nobody /usr/local/bin/nexd nexd /var/www/nex
```

Replace `/var/www/nex` with your content directory.

3. **Reload inetd**:

```bash
sudo killall -HUP inetd
```

### With Xinetd

Create `/etc/xinetd.d/nex`:

```
service nex
{
    disable         = no
    socket_type     = stream
    protocol        = tcp
    port            = 1900
    wait            = no
    user            = nobody
    server          = /usr/local/bin/nexd
    server_args     = /var/www/nex
    log_on_failure  += USERID
}
```

Then reload xinetd:

```bash
sudo systemctl reload xinetd
```

### With Systemd Socket Activation

Create `/etc/systemd/system/nex.socket`:

```ini
[Unit]
Description=Nex Protocol Socket

[Socket]
ListenStream=1900
Accept=yes

[Install]
WantedBy=sockets.target
```

Create `/etc/systemd/system/nex@.service`:

```ini
[Unit]
Description=Nex Protocol Server

[Service]
ExecStart=/usr/local/bin/nexd /var/www/nex
StandardInput=socket
StandardOutput=socket
User=nobody
```

Enable and start:

```bash
sudo systemctl enable nex.socket
sudo systemctl start nex.socket
```

## Testing

Run the comprehensive test suite:

```bash
make test
```

The test suite validates:
- File serving with correct MIME types
- Directory listings
- Security (directory traversal prevention)
- Error handling
- Edge cases

## Security

- **Path Canonicalization**: Uses `realpath()` to resolve paths and prevent directory traversal
- **Base Directory Validation**: Ensures all served paths are within the specified directory
- **Hidden File Protection**: Does not list files starting with `.`
- **No Execution**: Only serves static files, no code execution

## Development

### Project Structure

```
.
├── nexd.c          # Main server implementation
├── Makefile        # Build configuration
├── test.sh         # Test suite
└── README.md       # Documentation
```

### Code Style

- C99 standard compliance
- POSIX API usage
- Clear function separation
- Comprehensive error handling

## Links

- [Nex Protocol Specification](https://nightfall.city/nex/info/specification.txt)
- [Reference Implementation](https://hg.sr.ht/~m15o/nexd/)

## License

See file LICENSE.txt.

## Contributing

Contributions are welcome! Please ensure:
- Code compiles with `-std=c99 -Wall -Wextra -pedantic`
- All tests pass (`make test`)
- New features include corresponding tests

## Author

Dr. Olav Schettler

## AI Use

The code was created with the help of Cline & Anthropic Claude Sonnet 4.5.
