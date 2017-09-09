# toymon
System monitor (not mature but like a toy)

You would need following packages.
- libxcb-devel
- cairo-devel
- pango-devel
- librsvg2-devel

To build:

1. run ./autogen.sh shell script.

This generates configure shell script.

2. run ./configure shell script.

You may want to pass --prefix option.

3. make && make install
