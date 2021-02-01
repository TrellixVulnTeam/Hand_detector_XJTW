const dgram = require('dgram');
const { PORT, MQTT } = require('./env.js');
const mqtt = require('mqtt')

let client = mqtt.connect(MQTT)
console.log(`connecting to ${MQTT}`);

client.on("error", (err)=>{
    console.log(`~~~ failed to connect to ${MQTT}, exiting.`);
    console.log(`[ERROR] ${err}`);
    process.exit();
})

client.on('connect', function () {
  console.log("~~~ connected.");
})

const server = dgram.createSocket({
  type: "udp4",
  reuseAddr: true,
});

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, info) => {
  // console.log(`server got: ${msg} from ${info.address}:${info.port}`);
  if(client.connected) {
      client.publish("fingers",msg);
  }
});

server.on('listening', () => {
  const address = server.address();
  console.log(`listening for udp packets on ${address.address}:${address.port}`);
});

server.bind(PORT);