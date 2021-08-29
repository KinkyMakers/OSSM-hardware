Usage
------

The included examples sketches will walk you through all of the features of the library.
They can be used on all platforms, but they default to WiFi. To change between platforms,
you will need to change two lines of code in the `config.h` tab of the example.
It is recommended that you start with one of the Adafruit WiFi feathers before
moving on to cellular or ethernet.

For all examples, you will need to set `IO_USERNAME` and `IO_KEY` in the `config.h` tab.

The following sections demonstrate how to switch between WiFi, cellular, and ethernet.

WiFi (ESP8266, M0 WINC1500, WICED)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If you are using the included examples, you do not need to change anything for the Adafruit WiFi Feathers.
All WiFi based Feathers (ESP8266, M0 WiFi, WICED) will work with the examples out of the box.

You will need to add your WiFi information to the `WIFI_SSID` and `WIFI_PASS` defines in the `config.h` tab.

Cellular (32u4 FONA)
~~~~~~~~~~~~~~~~~~~~~
For FONA, you will only need to change from the default WiFi constructor to the FONA specific constructor in the `config.h` tab.
The rest of the sketch remains the same.

You will need to comment out these WiFi lines in `config.h`:

.. code-block:: c

   #include "AdafruitIO_WiFi.h"
   AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);


and uncomment the FONA lines in `config.h`:


.. code-block:: c

   #include "AdafruitIO_FONA.h"
   AdafruitIO_FONA io(IO_USERNAME, IO_KEY);


If your carrier requires APN info, you can set it by adding a call to `io.setAPN()` after `io.connect()` in the `setup()` function of the sketch.


.. code-block:: c

    void setup() {

      // start the serial connection
      Serial.begin(115200);

      // connect to io.adafruit.com
      io.connect();

      io.setAPN(F("your_apn"), F("your_apn_user"), F("your_apn_pass"));

    }


Ethernet (Ethernet FeatherWing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For Ethernet, you will only need to change from the default WiFi constructor to the Ethernet specific constructor in the `config.h` tab.
The rest of the sketch remains the same.

You will need to comment out these WiFi lines in `config.h`:


.. code-block:: c

    #include "AdafruitIO_WiFi.h"
    AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

and uncomment the Ethernet lines in `config.h`:


.. code-block:: c

    #include "AdafruitIO_Ethernet.h"
    AdafruitIO_Ethernet io(IO_USERNAME, IO_KEY);
