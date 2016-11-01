# Preamble

Forked from https://github.com/modeswitch/node-webrtc
MediaStream APIs are supported and partly implemented. Fake file audio devices are used instead of the hardware audio devices, enabling running this multiple times on one machine.

This open-source project provides a native module for NodeJS that supports a subset of standards-compliant WebRTC features. Specifically, the PeerConnection and DataChannel APIs. 

This project relies on precompiled WebRTC binaries.

# Getting Started

## Prerequisites

* python2
* git
* pkg-config
* libncurses-devel
* libssl-devel
* libnss-devel
* libexpat-devel

### Debian/Ubuntu

To install prerequisites with the apt-get package manager,

`apt-get install python2.7 git-all pkg-config libncurses5-dev libssl-dev libnss3-dev libexpat-dev  `

### CentOS/Fedora/RHEL

To install prerequisites with the yum package manager,

`yum install python git pkgconfig openssl-devel ncurses-devel nss-devel expat-devel`

### OSX

* XCode

To install prerequisites using the homebrew package manager,

```
brew update
brew install git openssl pkg-config openssl homebrew/dupes/ncurses nss expat
```

Some of these libraries may be installed by default. Package names for these libraries may differ between distributions.

## Install

The easiest way to install is via npm:

````
npm install wrtc-full
````

If you want to work from source:

````
git clone https://github.com/alexanderb14/node-webrtc-full
cd node-webrtc-full
npm install
````

## Troubleshooting

### Error while loading libtinfo.so.5 on Linux

Add a symlink to libncurses:

        sudo ln -s /usr/lib/libncurses.so.5 /usr/lib/libtinfo.so.5

### Some linux distros default to Python3 and the build process fails

This is most common on Arch Linux. Set `python2` as the default for `npm`:

        npm config set python python2

### The node.js package for my distro is too old

Follow the instructions here: https://github.com/joyent/node/wiki/installing-node.js-via-package-manager

# Tests

## Unit tests

Once everything is built, try `npm test` as a sanity check.

## bridge.js
You can run the data channel demo by `node examples/bridge.js` and browsing to `examples/peer.html` in `chrome --enable-data-channels`.

usage:
````
node examples/bridge.js [-h <host>] [-p <port>] [-ws <ws port>]
````
options:
````
-h  host IP for the webserver that will serve the static files (default 127.0.0.1)
-p  host port for the webserver that will serve the static files (default 8080)
-ws port of the Web Socket server (default 9001)
````

If the bridge and peer are on different machines, you can pass the bridge address to the peer by:
````
http://<webserver>/peer.html?<sockertserver:port>
````
By default the bridge will be the same IP as the webserver and will listen on port 9001.

## ping-pong-test.js

The ping-pong example creates two peer connections and sends some data between them.

usage:
````
node examples/ping-pong-test.js
````

[wrtc-gratipay-image]: https://img.shields.io/gratipay/modeswitch.svg?style=flat
[wrtc-gratipay-url]: https://gratipay.com/modeswitch/
