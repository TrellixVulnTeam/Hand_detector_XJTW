sudo apt-get install -y openssl libssl-dev

TEMP_FOLDER="`pwd`/__tmp__"
if [ ! -d $TEMP_FOLDER ]; then
    if mkdir $TEMP_FOLDER && cd $TEMP_FOLDER; then
        echo -n "~~ install paho.mqtt.c"
        
        git clone https://github.com/eclipse/paho.mqtt.c.git &&
            cd paho.mqtt.c &&
            git checkout -b v1.3.8 &&
            mkdir build &&
            cd build && 
            cmake .. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON && 
            cmake --build .
            sudo make install
            # cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON &&
            # sudo cmake --build build/ --target install && 
            # sudo ldconfig

        echo -n "~~ install paho.mqtt.cpp"
        git clone https://github.com/eclipse/paho.mqtt.cpp &&
            cd paho.mqtt.cpp &&
            git checkout -b v1.2.0 &&
            mkdir build &&
            cd build && 
            cmake .. -DPAHO_BUILD_STATIC=ON && 
            cmake --build . &&
            sudo make install
            # cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON
        
        echo -n "~~ remove temp folder"
        sudo rm -r $TEMP_FOLDER
    else 
        echo -n "could not create $TEMP_FOLDER folder to store installations"
    fi
else
    echo -n "$TEMP_FOLDER folder already exists"
fi

