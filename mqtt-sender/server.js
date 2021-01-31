const dgram = require('dgram');
const env = require('dotenv').config();
const mqtt = require('mqtt')

const PORT = env.PORT || 41234
const MQTT_SERVER = env.MQTT || 'mqtt://test.mosquitto.org';

var client  = mqtt.connect(MQTT_SERVER)
 
client.on("error", ()=>{
    console.log(`failed to connect to ${MQTT_SERVER}, exiting`);
    process.exit();
})

client.on('connect', function () {
  client.subscribe('presence', function (err) {
    if (!err) {
      client.publish('presence', 'Hello mqtt')
    }
  })
})

const server = dgram.createSocket('udp4');

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, info) => {
  console.log(`server got: ${msg} from ${info.address}:${info.port}`);
  if(client.connected) {
      client.publish("fingers",msg);
  }
});

server.on('listening', () => {
  const address = server.address();
  console.log(`listening for udp packets on ${address.address}:${address.port}`);
});


server.bind(PORT);