## Sample clients for the WTS Project ##

### Quick Start ###

#### Arduino client

Get a Node ID for your Arduino at:
http://wts.3open.org/tiny/getNodeID

Get the uno client and put your Node ID in the sketch.

Upload the sketch to your device and open the serial console for trouble shooting.

Remote control your device with **curl** or any http clients of your choice:

__Set digital pin 8 HIGH:__
> $ curl -d "node_id=your_node_id&payload=dw 8 1" http://wts.3open.org/tiny/api/node

__Set digital pin 8 LOW:__
> $ curl -d "node_id=your_node_id&payload=dw 8 0" http://wts.3open.org/tiny/api/node

__Read all sensor values:__
> $ curl -d "node_id=your_node_id&payload=rs" http://wts.3open.org/tiny/api/node

### Next Step ###

* Customize the payload for your own needs.
* Comment and report bugs.

