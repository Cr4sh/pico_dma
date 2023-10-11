
# Pico DMA

## General information

This design allows to perform fully autonomous pre-boot DMA attacks over PCI Express bus using Microblaze soft-processor with embedded software stack running on PicoEVB development board with Xilinx Artix 7 FPGA. Using Pico DMA it's possible to create hardware implants in format of tiny M.2 2230 card that injects my [Hyper-V backdoor](https://github.com/Cr4sh/s6_pcie_microblaze/tree/master/python/payloads/DmaBackdoorHv), [SMM Backdoor Next Gen](https://github.com/Cr4sh/SmmBackdoorNg) or any other UEFI DXE driver as payload into the target machine boot sequence.

Despite being focused on autonomous operation Pico DMA alternatively can be controlled over the UART interface in fully compatible way with [PCI Express DIY hacking toolkit](https://github.com/Cr4sh/s6_pcie_microblaze) software libraries and programs. The toolkit is also [providing](https://github.com/Cr4sh/s6_pcie_microblaze/blob/master/python/evb_ctl.py) `evb_ctl.py` program used by this design to flash PicoEVB bitstream and payload images into the on-board SPI flash chip of the board.


## About

Developed by:<br />
Dmytro Oleksiuk (aka Cr4sh)

[cr4sh0@gmail.com](mailto:cr4sh0@gmail.com)<br />
[http://blog.cr4.sh](http://blog.cr4.sh)
