Welcome to your first Connext DDS example!

This Python example was generated using the data type HelloWorld
from hello_world.idl.

This example has one application (hello_world_program.py)
which can run a publisher example (hello_world_publisher.py)
or a subscriber example (hello_world_subscriber.py).

The available examples are:

    - Default example - Basic publisher and subscriber
    - Advanced example (add -exampleTemplate advanced) - Uses an
    		asynchronous generator to read, and monitors status
    		updates in the DataWriter and DataReader.


To Modify the Data:
===================

To modify the data being sent edit the hello_world_publisher.py
file where it says

# Modify the data to be sent here


To Run this Example (command line)
==================================

Run the subscriber on one command prompt:

$ python hello_world_program.py --sub 
or
$ python hello_world_subscriber.py

Run the publisher on a different command prompt:

$ python hello_world_program.py --pub
or
$ python hello_world_publisher.py

You can pass additional arguments. To see the full list:

$ python hello_world_program.py --help

For example, to publish 10 data samples on domain 100, run the following:

$ python hello_world_program.py --pub --domain 100 --sample-count 10



