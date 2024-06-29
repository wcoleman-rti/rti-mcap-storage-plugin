
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

class HelloWorldSubscriber:

    @staticmethod
    def process_data(reader):
        # take_data() returns copies of all the data samples in the reader
        # and removes them. To also take the SampleInfo meta-data, use take().
        # To not remove the data from the reader, use read_data() or read().
        samples = reader.take_data()
        for sample in samples:
            print(f"Received: {sample}")
    
        return len(samples)

    @staticmethod
    def run_subscriber(domain_id: int, sample_count: int):

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
            "hello_world_ParticipantLibrary::hello_worldSubscriberParticipant",
            params
        )

        # Lookup the DataReader from the configuration
        reader = dds.DataReader(
            participant.find_datareader("hello_worldSubscriber::hello_worldDataReader")
        )

        # Enable the participant and underlying entities recursively
        participant.enable()

        # Initialize samples_read to zero
        samples_read = 0

        # Associate a handler with the status condition. This will run when the
        # condition is triggered, in the context of the dispatch call (see below)
        # condition argument is not used
        def condition_handler(_):
            nonlocal samples_read
            nonlocal reader
            samples_read += HelloWorldSubscriber.process_data(reader)

        # Obtain the DataReader's Status Condition
        status_condition = dds.StatusCondition(reader)

        # Enable the "data available" status and set the handler.
        status_condition.enabled_statuses = dds.StatusMask.DATA_AVAILABLE
        status_condition.set_handler(condition_handler)

        # Create a WaitSet and attach the StatusCondition
        waitset = dds.WaitSet()
        waitset += status_condition

        while samples_read < sample_count:
            # Catch control-C interrupt
            try:
                # Dispatch will call the handlers associated to the WaitSet conditions
                # when they activate
                print("Hello World subscriber sleeping for 1 seconds...")

                waitset.dispatch(dds.Duration(1))  # Wait up to 1s each time
            except KeyboardInterrupt:
                break

        print("preparing to shut down...")


if __name__ == "__main__":
    HelloWorldSubscriber.run_subscriber(
            domain_id=0,
            sample_count=sys.maxsize)
