## About
This repo contains the IoTConnect C SDK and samples for Azure Sphere, 
for MediaTek MT3620 MCU chipset and Guardian hardware that contains this chipset.

You may also be interested in:
- https://github.com/Azure/azure-sphere-hardware-designs/ - maintained hardware designs for Azure Sphere
- https://github.com/Azure/azure-sphere-gallery/ - gallery of further unmaintained content from Microsoft

Please also see the Codethink, MediaTek, and Azure RTOS repositories for more sample applications for the MT3620 real-time cores:
- https://github.com/CodethinkLabs/mt3620-m4-samples
- https://github.com/MediaTek-Labs/mt3620_m4_software
- https://github.com/Azure-Samples/Azure-RTOS-on-Azure-Sphere-Mediatek-MT3620

## Using the samples
See the [Azure Sphere Getting Started](https://www.microsoft.com/en-us/azure-sphere/get-started/) page for details on getting an [Azure Sphere development kit](https://aka.ms/AzureSphereHardware) and setting up your PC for development. You should complete the Azure Sphere [Installation Quickstarts](https://docs.microsoft.com/azure-sphere/install/overview) and [Tutorials](https://docs.microsoft.com/azure-sphere/install/qs-overview) to validate that your environment is configured properly before using the samples here. 

Download the project files from Github Actions in this repo.

See the [BUILD_INSTRUCTIONS](BUILD_INSTRUCTIONS.md) page for details on setting up the Azure Sphere development eniviroment and build/debug an Azure Sphere project.

## Port the samples to Guardian 700 hardware
Follow the steps below to port the sample's project to Guardian 700 hardware:
- In app_manifest.json file, add line ***"NetworkConfig": true*** to the ***Capabilities*** section.
- In CMakeLists.txt file, replace the definition of ***TARGET_DIRECTORY*** from ***"../../hardware-definitions/mt3620_rdb"*** to ***"../../hardware-definitions/techware_mt3620_slte"*** in azsphere_target_hardware_definition() macro. 
- In main.c file, replace the string of static character array ***network_interface*** from ***"wlan0"*** to ***"eth0"***. 

## License
For information about the licenses that apply to a particular sample, see the License file in each subdirectory.
