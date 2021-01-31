licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "paho_mqtt_c",
    srcs = glob(
        [
            "lib/libpaho-mqttpp3.so",
            "lib/libpaho-mqttpp3.a"
        ],
    ),
    hdrs = glob([
        "include/mqtt/**/*.h*",
    ]),
    includes = [
        "include/mqtt",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)
