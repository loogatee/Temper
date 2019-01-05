
from pyepsolartracer.client import EPsolarTracerClient
from pyepsolartracer.registers import Jregisters,coils
from pymodbus.client.sync import ModbusSerialClient as ModbusClient

# configure the client logging
import logging
logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

serialclient = ModbusClient(method='rtu', port='/dev/ttyXRUSB0', baudrate=115200, stopbits = 1, bytesize = 8, timeout=1)
#serialclient = None

client = EPsolarTracerClient(serialclient = serialclient)
client.connect()

response = client.read_device_info()
#print "Manufacturer:", repr(response.information[0])
#print "Model:", repr(response.information[1])
#print "Version:", repr(response.information[2])

#response = client.read_input("Charging equipment rated input voltage")
#print str(response)

vlist = []

for reg in Jregisters:
    #print
    print reg.address
    value = client.read_input(reg.name)
    vlist.append(value)
    #print value
    if reg.address == 36885:
        X = value.value + 1
        print reg.name
        print client.write_output(reg.name,X)

print ",".join(map(str,vlist))



client.close()
