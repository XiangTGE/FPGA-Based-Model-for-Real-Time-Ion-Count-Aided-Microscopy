# ADC Test

This tests Zmod Scope-105 on the Eclypse Z7. Associated hardware design files and firmware code is provided as well as Python code to read ADC outputs and display on terminal. Hardware developed on Vivado 2023.2. The hardware simply samples data from the ADC and sends that over via USB to host PC. 

Contains block diagram design and xci files for IP cores used, as well as module for generating TLAST signal to ensure that AXI DMA transfers work properly. 

For those looking to simply code on the hardware platform, the .xsa file is enough, and it should contain hardware configuration information required for firmware coding. 

NOTE: Currently, USB firmware may not work properly if you unplug and replug, you may have to reprogram the device if you accidentally disconnect the device. 