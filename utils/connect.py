import gatt
from gi.repository import GObject

from argparse import ArgumentParser


class AnyDevice(gatt.Device):
    c = None

    def connect_succeeded(self):
        super().connect_succeeded()
        print("[%s] Connected" % (self.mac_address))

    def connect_failed(self, error):
        super().connect_failed(error)
        print("[%s] Connection failed: %s" % (self.mac_address, str(error)))

    def disconnect_succeeded(self):
        super().disconnect_succeeded()
        print("[%s] Disconnected" % (self.mac_address))

    def services_resolved(self):
        super().services_resolved()

        print("[%s] Resolved services" % (self.mac_address))
        for service in self.services:
            print("[%s]  Service [%s]" % (self.mac_address, service.uuid))
            for characteristic in service.characteristics:
                print("[%s]    Characteristic [%s]" % (self.mac_address, characteristic.uuid))
                
                if characteristic.uuid == '4f9e0296-06f5-4808-adbb-36f719890301':
                    self.c = characteristic
                    self.c.enable_notifications()
    
    def characteristic_enable_notifications_succeeded(self, characteristic):
        self.c.write_value(b"1234")
    
    def characteristic_value_updated(self, characteristic, value):
        print("Received: " + repr(value))


arg_parser = ArgumentParser(description="GATT Connect Demo")
arg_parser.add_argument('mac_address', help="MAC address of device to connect")
args = arg_parser.parse_args()

print("Connecting...")

manager = gatt.DeviceManager(adapter_name='hci0')

device = AnyDevice(manager=manager, mac_address=args.mac_address)
device.connect()

manager.run()
