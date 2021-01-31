licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "paho_mqtt_c",
    srcs = glob(
        [
            "lib/libpaho-mqtt3c.so",
            "lib/libpaho-mqtt3c.a"
            "lib/libpaho-mqtt3a.so",
            "lib/libpaho-mqtt3a.a"
            "lib/libpaho-mqtt3cs.so",
            "lib/libpaho-mqtt3cs.a",
            "lib/libpaho-mqtt3as.so",
            "lib/libpaho-mqtt3as.a",
        ],
    ),
    hdrs = glob([
        # "include/mqtt/**/*.h*",
        "include/MQTTAsync.h",
        "include/MQTTClient.h",
        "include/MQTTClientPersistence.h",
        "include/MQTTProperties.h",
        "include/MQTTReasonCodes.h",
        "include/MQTTSubscribeOpts.h",
        "include/MQTTExportDeclarations.h",
    ]),
    includes = [
        "include/",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)
