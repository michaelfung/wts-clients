## Sample clients for the WTS Project ##

### Quick Start for Arduino ###

1. Get a Node ID for your Arduino at:
http://wts.3open.org/get_node_id.html

2. Get the Uno client and put your Node ID in the sketch.

3. Upload the sketch to your device and open the serial console for trouble shooting.

4. Remote control your device with **cURL** or any http clients of your choice.

### cURL Example:

```
API_BASE=http://wts.3open.org
NODE_ID=your_node_id
NODE_KEY=your_node_key # defaults to 'NOKEY'
```

__Set digital pin 4 HIGH:__

``` curl -X POST "${API_BASE}/t1/node/${NODE_ID}" -u "anon:${NODE_KEY}" -d "payload=dw 4 1" ```

__Set digital pin 4 LOW:__

``` curl -X POST "${API_BASE}/t1/node/${NODE_ID}" -u "anon:${NODE_KEY}" -d "payload=dw 4 0" ```

__Read all sensor values:__

``` curl -X POST "${API_BASE}/t1/node/${NODE_ID}" -u "anon:${NODE_KEY}" -d "payload=rs" ```


### HTML5 and Javascript Example:

<a href="http://wts.3open.org/tinyapi-demo.html" target="_blank">
    http://wts.3open.org/tinyapi-demo.html
</a>

### Next Step ###

* Customize the payload for your own needs.
* Comment and report bugs.
