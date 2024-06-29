
# (c) Copyright, Real-Time Innovations, 2022.  All rights reserved.
# RTI grants Licensee a license to use, modify, compile, and create derivative
# works of the software solely for use with RTI Connext DDS. Licensee may
# redistribute copies of the software provided that all such copies are subject
# to this license. The software is provided "as is", with no warranty of any
# type, including any warranty for fitness for any purpose. RTI is under no
# obligation to maintain or support the software. RTI shall not be liable for
# any incidental or consequential damages arising out of the use or inability
# to use the software.

import time
import sys
import rti.connextdds as dds
from hello_world import HelloWorld

class HelloWorldPublisher:

    @staticmethod
    def run_publisher(domain_id: int, sample_count: int):

        # When using user-generated types, you must register the type with RTI
        # Connext DDS before creating the participants and the rest of the entities
        # in your system
        dds.DomainParticipant.register_idl_type(HelloWorld, "HelloWorld")

        # Disable autoenabling DDS entities upon creation.
        # This allows easy enabling of entities under a participant recursively.
        dds.DomainParticipant.participant_factory_qos.entity_factory.autoenable_created_entities = False

        # Create the participant, changing the domain id from the one in the
        # configuration
        params = dds.DomainParticipantConfigParams(domain_id)
        participant = dds.QosProvider.default.create_participant_from_config(
            "hello_world_ParticipantLibrary::hello_worldPublisherParticipant",
            params
        )

        # Lookup the DataWriter from the configuration
        writer = dds.DataWriter(
            participant.find_datawriter("hello_worldPublisher::hello_worldDataWriter")
        )

        # Enable the participant and underlying entities recursively
        participant.enable()

        num_instances = 10

        sample = HelloWorld()
        sample.message = "Hello!"

        for count in range(sample_count):
            # Catch control-C interrupt
            try:
                # Modify the data to be sent here
                sample.id = (count % num_instances)
                
                print(f"Writing HelloWorld: {sample}")
                writer.write(sample)
                time.sleep(0.1)
            except KeyboardInterrupt:
                break

        print("preparing to shut down...")


if __name__ == "__main__":
    HelloWorldPublisher.run_publisher(
            domain_id=0,
            sample_count=sys.maxsize)
