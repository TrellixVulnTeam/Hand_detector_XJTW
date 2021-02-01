const path = require("path");

let p = path.resolve(path.join(__dirname,"../.env"));
console.log({p});
let dotenv = require('dotenv').config({
  path: p
});

module.exports= {
  PORT: process.env.PORT || dotenv.PORT || 41234,
  MQTT: process.env.MQTT || dotenv.MQTT || "mqtt://test.mosquitto.org",
};

console.log({env: module.exports});
