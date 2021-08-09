# IoTConnect C SDK for Azure Sphere OS
This repo contains the IoTConnect C SDK and samples for Azure Sphere, 
for MediaTek MT3620 MCU chipset and Guardian hardware that contains this chipset.

You may also be interested in:
- https://github.com/Azure/azure-sphere-hardware-designs/ - maintained hardware designs for Azure Sphere
- https://github.com/Azure/azure-sphere-gallery/ - gallery of further unmaintained content from Microsoft

Please also see the Codethink, MediaTek, and Azure RTOS repositories for more sample applications for the MT3620 real-time cores:
- https://github.com/CodethinkLabs/mt3620-m4-samples
- https://github.com/MediaTek-Labs/mt3620_m4_software
- https://github.com/Azure-Samples/Azure-RTOS-on-Azure-Sphere-Mediatek-MT3620

# Using the samples
See the [Azure Sphere Getting Started](https://www.microsoft.com/en-us/azure-sphere/get-started/) page for details on getting an [Azure Sphere development kit](https://aka.ms/AzureSphereHardware) and setting up your PC for development. You should complete the Azure Sphere [Installation Quickstarts](https://docs.microsoft.com/azure-sphere/install/overview) and [Tutorials](https://docs.microsoft.com/azure-sphere/install/qs-overview) to validate that your environment is configured properly before using the samples here. 

## Download or clone the samples
- You can download the samples zip release from this repository Release page or clone it to your development PC.
- To clone the samples repository:
  - install Git Client from https://git-scm.com/downloads onto your development PC.
  - Open a console and clone the complete samples repo to your development PC with the following command:
    - *git clone --depth 1 --recurse-submodules git://github.com/avnet-iotconnect/iotc-sphereos-sdk.git*

## Open and build the samples with Microsoft Visual Studio 
- Launch Visual Studio and open a sample project folder. *(eg: C:\projects\iotc-sphereos-sdk\samples\basic-sample)*
- The Visual Studio should auto generate cmake cache when sample project folder is opened. If not, right-click on the *CMakeLists.txt* from the **Solution Explorer** panel and select **Generate cache for xxxx-sample** where *xxxx-sample* is the project name of the sample project folder that is opened. *(eg: basic-sample)*
- Once cache generated successfully, click on **Build All** from the **Build** menu to build the sample project.

## Open and build the samples with Microsoft Visual Studio Code
- Launch Visual Studio Code and open a sample project folder. *(eg: C:\projects\iotc-sphereos-sdk\samples\basic-sample)*
- Right-click on the *CMakeLists.txt* from the **Explorer** panel and select **Configure All Projects** to generate cmake cache.
- Once cache generated successfully, right-click on *CMakeLists.txt* from the **Explorer** panel and select **Build All Projects** to build the sample project.

# Porting the samples to Techware Guardian 700 hardware
- In app_manifest.json file, add line ***"NetworkConfig": true*** to the ***Capabilities*** section.
- In CMakeLists.txt file, replace the definition of ***TARGET_DIRECTORY*** from ***"../../hardware-definitions/mt3620_rdb"*** to ***"../../hardware-definitions/techware_mt3620_slte"*** in azsphere_target_hardware_definition() macro. 
- In main.c file, replace the string of static character array ***network_interface*** from ***"wlan0"*** to ***"eth0"***. 

*Note: For porting to other Guardian hardware, set **TARGET_DIRECTORY** to the hardware definition directory that contains the respective Guardian hardware definition.*

# License
For information about the licenses that apply to a particular sample, see the License file in each subdirectory.
