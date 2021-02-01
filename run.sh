#!/bin/bash
cd mqtt-sender && npm run start &
pids="$pids $!"

GLOG_logtostderr=1 bazel-bin/mediapipe/examples/desktop/hand_tracking/hand_tracking_out_gpu --calculator_graph_config_file=mediapipe/graphs/hand_tracking/hand_tracking_desktop_live_gpu.pbtxt &
pids="$pids $!"

echo -e "processes: $pids"
stop() {
  echo "Caught signal!" 
  for pid in $pids; do
    kill -9 $pid 2>/dev/null
  done
  exit
}
trap stop SIGINT
wait -n $pids
stop