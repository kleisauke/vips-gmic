# vips-gmic

libvips plugin for running gmic commands.

Build the plugin and install to `$VIPSHOME/lib/vips-plugins-8.10` (or whatever
version you are using) with:
```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$VIPSHOME
make -j$(nproc)
sudo make plugin-install
```

Then run with eg.:
```bash
vips gmic ~/pics/k2.jpg x.jpg 20 1 1 -- "-verbose - -blur 10"
```
