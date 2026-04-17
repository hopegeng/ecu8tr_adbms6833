import sys
import getopt
import struct

# Default source address
ID_SA = 0x0a
# Default Target address
ID_TA = 0x0b

class TLE9012:
    OPTION_CONTROL = 0x03
    UDS_IOCBI = 0x2F
    ID1 = 0x00
    ID3 = 0x03
    
    simulation_flow = 0x01
    real_flow = 0x02
    connection_state_start = 0x01
    connection_state_stop = 0x00


    def __init__( self ):
        pass  

    def getMetric(self, nodeNum):
        ID2 = 0x0B

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", nodeNum )
        return frame
    
    def getMetric2(self, nodeNum):
        ID2 = 0x09

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", nodeNum )
        return frame
    
    def cyclingEnable(self, cycleEnable):
        ID2  = 0X21

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", cycleEnable )
        return frame        

    def chargeRelay(self, relayEnable):
        ID2  = 0X0C

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", relayEnable )
        return frame

    def dischargeRelay(self, relayEnable):
        ID2  = 0X0D

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", relayEnable )
        return frame

    def sim_connect(self, connection_state, num_nodes, cell_capacity_high, cell_capacity_low):
        ID2  = 0X01

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID3 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", connection_state )
        frame += struct.pack( ">B", num_nodes )
        frame += struct.pack( ">B", cell_capacity_high )
        frame += struct.pack( ">B", cell_capacity_low )
        return frame

    def sim_send_cell_cata(self, connectionState, numNodes, cellCapacityH, cellCapacityL):
        ID2  = 0X01

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID3 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", connectionState )
        frame += struct.pack( ">B", numNodes )
        frame += struct.pack( ">B", cellCapacityH )
        frame += struct.pack( ">B", cellCapacityL )
        return frame

    def sim_get_cell_data(self, node_num):
        ID2 = 0x03

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID3 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )

        frame += struct.pack( ">B", node_num )
        return frame

    def sim_get_status(self):
        ID2 = 0x04

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID3 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )
        return frame

    def sim_flow_control(self, flow_sim_real, fuel_gauge, numOfNodes):
        ID2 = 0x1f

        frame = bytearray()
        frame += struct.pack( ">B", TLE9012.UDS_IOCBI )
        frame += struct.pack( ">B", TLE9012.ID1 )
        frame += struct.pack( ">B", ID2 )
        frame += struct.pack( ">B", TLE9012.OPTION_CONTROL )
        frame += struct.pack( ">B", flow_sim_real )     
        frame += struct.pack( ">B", fuel_gauge )   
        frame += struct.pack( ">B", numOfNodes )      
        return frame
