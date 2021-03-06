import gatt
from gi.repository import GObject
import time

from argparse import ArgumentParser

infile = open("speech8kHz.gsm", "rb")
#infile = open("zeros.gzm", "rb")
outfile = open("outdata.bin", "wb")

class AnyDevice(gatt.Device):
    c = None
    total_rx = 0
    start = 0
    count = 0

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
                    self.start = time.time()
                    GObject.timeout_add(20, self.send)
    
    def characteristic_enable_notifications_succeeded(self, characteristic):
        pass
    
    def characteristic_value_updated(self, characteristic, value):
        self.total_rx += len(value)
        print("%10.3f: %8d %8d %8d B/s, idx %d" % (time.time() - self.start, self.total_rx, len(value), self.total_rx/(time.time() - self.start), value[0]))
        #outfile.write(bytes([len(value)]) + value)
        outfile.write(value[1:])
        outfile.flush()
    
    def send(self):
        self.count = (self.count + 1) & 255
        print("%10.3f: Sending packet %d" % (time.time() - self.start, self.count))
        packet = infile.read(33)
        if packet:
            self.c.write_value(bytes([self.count]) + packet)
            GObject.timeout_add(20, self.send)
        else:
            self.disconnect()



arg_parser = ArgumentParser(description="GATT Connect Demo")
arg_parser.add_argument('mac_address', help="MAC address of device to connect")
args = arg_parser.parse_args()

print("Connecting...")

manager = gatt.DeviceManager(adapter_name='hci0')

device = AnyDevice(manager=manager, mac_address=args.mac_address)
device.connect()

manager.run()
