
| Tested Targets | ESP32-C3 |
| ----------------- | -------- |

# ESP-MQTT_OTA_EXAMPLE

This project aims to demonstrate how to implement Over-The-Air (OTA) firmware updates for IoT devices using the MQTT (Message Queuing Telemetry Transport) protocol.OTA updates play a crucial role in the maintenance and improvement of IoT devices by enabling remote firmware upgrades, bug fixes, and feature enhancements without the need for physical intervention.

## Requirments

* ESP-IDF 5.1
* MQTTX (MQTT Testing Envoirment)
* Google Drive (OTA binary file)

## Getting Started 

1. Clone the repository to your local machine using git clone <repository-url>.
2. In VScode, VScode configuration folder need to be generated.
 - View -> Command Pallette -> ESP-IDF: Add vscode configuration folder
### Configure the project 
* Open the project configuration menu (`idf.py menuconfig`) and configure the project
* Serial Flasher Config -> Flash size -> 4 MB
* Partion Table -> "Factory app, two OTA definition"
* Example Connection Configuration -> set "Wifi SSID" and "Wifi Password"
* Component Config -> ESP-TLS -> Alow Potentially insecure options -> Skip server Certification (Checked)
* Component Config -> ESP HTTPS OTA -> Allow HTTP for OTA (Checked)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:


```
idf.py -p PORT flash monitor

```

### Testing MQTT and OTA

#### Step 1: Understanding MQTT Protocol
"In MQTTX, we will send our messages to the topic `test/empa/message`. This code will parse the received message to determine its type. Below is the template of the recommended message:
```
{
  "type": 0,
  "message" : "Hello From MQTTX"
}
```

Type 0 is for normal messages, where the device will only print them to the monitor. For OTA, we need to use type 1 and provide a "Direct Download Link" in the message section.
```
{
  "type": 1,
  "message" : "https://drive.google.com/uc?export=download&id=1VJMIi8EavjD51C83VHVzzSe0EyNRrk0d"
}
```

#### Step 2: Generating a Direct Link For OTA

To download an OTA (Over-The-Air) update, ESP needs a direct download link. We can use Google Drive to store the binary file. Then, to generate a direct download link, we can use this site (Note are important in below side):
```
https://sites.google.com/site/gdocs2direct/
```

When you have the direct link, you can pass it to the MQTT message and publish it to the device.

## Example Output

```
I (21436) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:f612:faff:fe15:e5e4, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (23566) esp_netif_handlers: example_netif_sta ip: 192.168.0.43, mask: 255.255.255.0, gw: 192.168.0.1
I (23566) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.0.43
I (23566) example_common: Connected to example_netif_sta
I (23576) example_common: - IPv4 address: 192.168.0.43,
I (23586) example_common: - IPv6 address: fe80:0000:0000:0000:f612:faff:fe15:e5e4, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (23596) MQTT_EXAMPLE: Other event id:7
I (23826) wifi:<ba-add>idx:1 (ifx:0, 8e:dc:96:9b:f6:1e), tid:0, ssn:0, winSize:64
I (24436) MQTT_EXAMPLE: MQTT_EVENT_CONNECTED
I (24446) MQTT_EXAMPLE: sent subscribe successful, msg_id=33610
I (24746) MQTT_EXAMPLE: MQTT_EVENT_SUBSCRIBED, msg_id=33610
I (2933166) MQTT_EXAMPLE: MQTT_EVENT_DATA
TOPIC=test/empa/message
DATA={
  "type": 1,
  "message" : "https://drive.google.com/uc?export=download&id=1VJMIi8EavjD51C83VHVzzSe0EyNRrk0d"
}
I (2933596) MQTT_EXAMPLE: OTA JOB is Received
I (2933766) MQTT_EXAMPLE: Attempting to download update from https://drive.google.com/uc?export=download&id=1VJMIi8EavjD51C83VHVzzSe0EyNRrk0d
W (2933776) esp_https_ota: Continuing with insecure option because CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP is set.
I (2937786) esp_https_ota: Starting OTA...
I (2937786) esp_https_ota: Writing to partition subtype 16 at offset 0x110000
I (2956586) esp_image: segment 0: paddr=00110020 vaddr=3c0c0020 size=28398h (164760) map
I (2956606) esp_image: segment 1: paddr=001383c0 vaddr=3fc90600 size=028b4h ( 10420) 
I (2956606) esp_image: segment 2: paddr=0013ac7c vaddr=40380000 size=0539ch ( 21404) 
I (2956616) esp_image: segment 3: paddr=00140020 vaddr=42000020 size=b11d0h (725456) map
I (2956706) esp_image: segment 4: paddr=001f11f8 vaddr=4038539c size=0b17ch ( 45436) 
I (2956716) esp_image: segment 0: paddr=00110020 vaddr=3c0c0020 size=28398h (164760) map
I (2956746) esp_image: segment 1: paddr=001383c0 vaddr=3fc90600 size=028b4h ( 10420) 
I (2956746) esp_image: segment 2: paddr=0013ac7c vaddr=40380000 size=0539ch ( 21404) 
I (2956746) esp_image: segment 3: paddr=00140020 vaddr=42000020 size=b11d0h (725456) map
I (2956846) esp_image: segment 4: paddr=001f11f8 vaddr=4038539c size=0b17ch ( 45436) 
I (2956906) MQTT_EXAMPLE: OTA Succeed, Rebooting...
I (2956906) wifi:state: run -> init (0)
I (2956906) wifi:pm stop, total sleep time: 2689519450 us / 2937350267 us

I (2956906) wifi:<ba-del>idx:1, tid:0
I (2956906) wifi:<ba-del>idx:0, tid:6
I (2956916) wifi:new:<4,0>, old:<4,0>, ap:<255,255>, sta:<4,0>, prof:1
I (2956926) wifi:flush txq
I (2956926) wifi:stop sw txq
I (2956926) wifi:lmac stop hw txq
I (2956926) wifi:Deinit lldesc rx mblock:10
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x3 (RTC_SW_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
Saved PC:0x40048b82
0x40048b82: ets_secure_boot_verify_bootloader_with_keys in ROM

SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5820,len:0x1708
load:0x403cc710,len:0x968
load:0x403ce710,len:0x2f68
entry 0x403cc710
I (35) boot: ESP-IDF v5.1-dirty 2nd stage bootloader
I (35) boot: compile time Aug  7 2023 08:43:14
I (35) boot: chip revision: v0.3
I (38) boot.esp32c3: SPI Speed      : 80MHz
I (43) boot.esp32c3: SPI Mode       : DIO
I (48) boot.esp32c3: SPI Flash Size : 4MB
I (53) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (84) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  3 factory          factory app      00 00 00010000 00100000
I (99) boot:  4 ota_0            OTA app          00 10 00110000 00100000
I (106) boot:  5 ota_1            OTA app          00 11 00210000 00100000
I (114) boot: End of partition table
I (118) esp_image: segment 0: paddr=00110020 vaddr=3c0c0020 size=28398h (164760) map
I (153) esp_image: segment 1: paddr=001383c0 vaddr=3fc90600 size=028b4h ( 10420) load
I (155) esp_image: segment 2: paddr=0013ac7c vaddr=40380000 size=0539ch ( 21404) load
I (163) esp_image: segment 3: paddr=00140020 vaddr=42000020 size=b11d0h (725456) map
I (283) esp_image: segment 4: paddr=001f11f8 vaddr=4038539c size=0b17ch ( 45436) load
I (297) boot: Loaded app from partition at offset 0x110000
I (297) boot: Disabling RNG early entropy source...
I (308) cpu_start: Unicore app
I (309) cpu_start: Pro cpu up.
I (317) cpu_start: Pro cpu start user code
I (317) cpu_start: cpu freq: 160000000 Hz
I (317) cpu_start: Application information:
I (320) cpu_start: Project name:     mqtt_tcp
I (325) cpu_start: App version:      d345fe0-dirty
I (331) cpu_start: Compile time:     Aug  7 2023 09:03:54
I (337) cpu_start: ELF file SHA256:  38e58c088413be2e...
ELF file not found. You need to build & flash the project before running 'monitor', and the binary on the device must match the one in the build directory exactly. 
I (343) cpu_start: ESP-IDF:          v5.1-dirty
I (348) cpu_start: Min chip rev:     v0.3
I (353) cpu_start: Max chip rev:     v0.99 
I (358) cpu_start: Chip rev:         v0.3
I (362) heap_init: Initializing. RAM available for dynamic allocation:
I (370) heap_init: At 3FC97D50 len 000449C0 (274 KiB): DRAM
I (376) heap_init: At 3FCDC710 len 00002950 (10 KiB): STACK/DRAM
I (382) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (390) spi_flash: detected chip: generic
I (393) spi_flash: flash io: dio
I (398) sleep: Configure to isolate all GPIO pins in sleep state
I (404) sleep: Enable automatic switching of GPIO sleep configuration
I (411) app_start: Starting scheduler on CPU0
I (416) main_task: Started on CPU0
I (416) main_task: Calling app_main()
I (426) MQTT_EXAMPLE: [APP] Startup..
I (426) MQTT_EXAMPLE: [APP] Free memory: 282332 bytes
I (426) MQTT_EXAMPLE: [APP] IDF version: v5.1-dirty
I (436) MQTT_EXAMPLE: ESP_OTA_VERSION

```
