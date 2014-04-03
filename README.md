## Sample clients for the WTS Project ##

### Quick Start for Arduino ###

1. Get a Node ID for your Arduino at:
http://wts.3open.org/tiny/getNodeID

2. Get the Uno client and put your Node ID in the sketch.

3. Upload the sketch to your device and open the serial console for trouble shooting.

4. Remote control your device with **cURL** or any http clients of your choice.

#### cURL Example:

__Set digital pin 4 HIGH:__
> $ curl -d "node_id=your_node_id&payload=dw 4 1" http://wts.3open.org/tiny/api/node

__Set digital pin 4 LOW:__
> $ curl -d "node_id=your_node_id&payload=dw 4 0" http://wts.3open.org/tiny/api/node

__Read all sensor values:__
> $ curl -d "node_id=your_node_id&payload=rs" http://wts.3open.org/tiny/api/node


#### HTML5 and Javascript Example:

<a href="http://wts.3open.org/tinyapi-demo.html" target="_blank">
    http://wts.3open.org/tinyapi-demo.html
</a>

### Next Step ###

* Customize the payload for your own needs.
* Comment and report bugs.

