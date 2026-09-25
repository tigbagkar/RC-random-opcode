
Preparation

You need to install soft roce and register at least one device

Running

Initiator and Target are two separate processes and must be run independently in two different terminal windows.
The Target process must be started !before! the Initiator process.

Terminal 1 - Target

make run-target
Wait until the Target process is ready.

Terminal 2 - Initiator

make run-initiator
The Initiator and Target processes communicate with each other over the network.

