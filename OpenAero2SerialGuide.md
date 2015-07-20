# Getting Started Guide #

## Where can I find OpenAero2Serial firmware? ##
The latest files are always listed at the bottom of the first post in the RCGroups OpenAero2 thread.

A link to the latest beta version of OpenAero2Serial can be found on [RC Groups.com](http://www.rcgroups.com/forums/showpost.php?p=22394780&postcount=1)

The following diagram shows the serial connections to the KK2 board.

![http://imgur.com/cY4xjWf.jpg](http://imgur.com/cY4xjWf.jpg)

## How do I connect the KK2 board in my model? ##
The KK2 board should be installed within your aircraft between your receiver and second flight controller, to stabilize your model.

### Powering the KK2 Board ###
A 5 volt regulated power supply must be connected to the pin marked M1. Typically, this will be the 5 volt supply from the ESC's internal BEC.
This will provide power to both the KK2 board and the receiver.

We also recommend the [Turnigy 3A 5V/6V](http://www.hobbyking.com/hobbyking/store/__4319__TURNIGY_3A_UBEC_w_Noise_Reduction.html) & [HobbyKing HKU5 5A 5V](http://www.hobbyking.com/hobbyking/store/__16663__HobbyKing_HKU5_5V_5A_UBEC.html) external UBECs from Hobbyking.

## How do I connect my receiver? ##
The OpenAero2Serial supports single-wire CCPM or Serial receivers (S.Bus, Satellite, Xtreme).

### CPPM Receivers ###
Single-wire receivers can be connected to the **THR** input for both signal and power.
While there are many brands available, the following have been tested.

  * [FrSky V8R7-SP](http://www.frsky-rc.com/ShowProducts.asp?id=48)
  * [FrSky D4R-II](http://www.frsky-rc.com/ShowProducts.asp?id=119)
  * [FrSky D8R-XP](http://www.frsky-rc.com/ShowProducts.asp?id=126)

### Serial receivers (IN) ###
Serial receivers also connect to the KK2 board via the **THR** input. Some of these require a cable adapter to function.

For the Spektrum Satellite, please use the following cable available from HobbyKing.
[ZYX-S DSM2/DSMJ Satellite Receiver Cable](http://www.hobbyking.com/hobbyking/store/__24524__ZYX_S_DSM2_DSMJ_Satellite_Receiver_Cable.html)

![http://www.hobbyking.com/hobbyking/store/catalog/24524-main.jpg](http://www.hobbyking.com/hobbyking/store/catalog/24524-main.jpg)

**Spektrum Satellite setup**

![http://i.imgur.com/At2S8zS.jpg](http://i.imgur.com/At2S8zS.jpg)

For the Futaba/OrangeRx S.Bus, please use the following cable available from HobbyKing.
[ZYX-S S.BUS Connection Cable](http://www.hobbyking.com/hobbyking/store/__24523__ZYX_S_S_BUS_Connection_Cable.html)

**Futaba S.bus setup**

![http://i.imgur.com/BMlI60q.jpg](http://i.imgur.com/BMlI60q.jpg)

### Serial receivers (OUT) ###
OpenAero2Serial outputs serial data from the "AIL" pin of the KK2 board. Some serial formats require a cable adapter to function.

| **Output format** | **Adapter** |
|:------------------|:------------|
|S.Bus              | Yes - Same as input adapter|
|Satellite          | No, but has 5V signal level|
|Xtreme             | No          |

### Binding satellite receivers ###
OpenAero2Serial makes it easy to bind Satellite receivers without the need for a separate host receiver.
  1. Ensure that the Satellite receiver is connected via the adapter cable.
  1. Hold down the relevant button of the KK2 board as per the table below.
  1. While holding down the button, apply power to the KK2 board. The attached Satellite receiver should start to blink rapidly. Release the button.
  1. Switch on the TX in binding mode. The is usually done by holding down the Bind button on the TX module. The TX module should flash.
  1. After a few seconds the Satellite will complete the binding process and will show a solid lit LED.

| **Button** | **Binding mode** |
|:-----------|:-----------------|
|Button 1    | DSM2 1024/22ms   |
|Button 2    | DSM2 2048/11ms   |
|Button 3    | DSMX 2048/22ms   |
|Button 4    | DSMX 2048/11ms   |

<a href='http://www.youtube.com/watch?feature=player_embedded&v=c_x0GtdCm6o' target='_blank'><img src='http://img.youtube.com/vi/c_x0GtdCm6o/0.jpg' width='425' height=344 /></a>

### Channel Order ###
OpenAero2Serial supports the JR/Spektrum and Fubata channel allocations.

| **JR/Spektrum** | **Futaba**|
|:----------------|:----------|
|CH1: Throttle    |CH1: Aileron #1|
|CH2: Aileron #1  |CH2: Elevator|
|CH3: Elevator    |CH3: Throttle|
|CH4: Rudder      |CH4: Rudder|
|CH5: Gear switch |CH5: Gear switch|
|CH6: Aux 1       |CH6: Aileron #2|
|CH7: Aux 2       |CH7: Aux 1 |
|CH8: Aux 3       |CH8: Aux 2 |

## How are the channels mapped? ##
The OpenAero2Serial mixer presets support either conventional 'aeroplane mode' or 'flying-wing mode'.
All unassigned channels, designated as 'none' by the mixer presets, can be manually customized for other purposes.
Channels between CH8 and CH16 are passed through as-is.

The following channel assignment will apply to most conventional aeroplanes.

  * 1: Throttle
  * 2: Aileron
  * 3: Elevator
  * 4: Rudder
  * 5: Gear
  * 6: Aux 1
  * 7: Aux 2
  * 8: Aux 3

## Using the mixer ##
OpenAero2Serial uses an updated mixer from OpenAeroVTOL and differs significantly from the older OpenAero2Serial Guides

### RC source A: ###
You can select between any of the eight primary input channels, THR to AUX3.
### Volume (%): ###
This is the amount of the selected RC channel that is added to the output.
### RC source B: ###
You can select between any of the eight primary input channels, THR to AUX3.
### Volume (%): ###
This is the amount of the selected RC channel that is added to the output.
### Roll gyro: ###
ON/OFF/Reverse.
### Pitch gyro: ###
ON/OFF/Reverse.
### Yaw gyro: ###
ON/OFF/Reverse.
### Roll acc: ###
ON/OFF/Reverse.
### Pitch acc: ###
ON/OFF/Reverse.
### Ext. Source: ###
There are 16 possible source inputs. CH1 to CH8, then the roll/pitch/yaw gyros, roll/pitch accelerometers and the roll/pitch autolevel (IMU)
### Volume (%): ###
This is the amount of the selected source that is added to the output.
### Ext. Source: ###
There are 16 possible source inputs. CH1 to CH8, then the roll/pitch/yaw gyros, roll/pitch accelerometers and the roll/pitch autolevel (IMU)
### Volume (%): ###
This is the amount of the selected source that is added to the output.