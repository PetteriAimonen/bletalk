#!/usr/bin/env python3
# Reads binary log from RAM

import sys
import time
import re
import struct
from openocd import nm, ocd

class LogPrinter:
    def __init__(self, server, symbols):
        self.server = server
        self.symbols = symbols
        self.binlog_addr = symbols.lookup_address("g_binlog")
        self.binlog_idx = symbols.lookup_address("g_binlog_idx")
        self.binlog_len = 64
        self.len_mask = self.binlog_len - 1
        self.idx = -1
        self.strings = {}
    
    def sprintf(self, fmt, args):
        format_parts = re.findall("%[-+0-9. #]*[xduf]", fmt)
        converted_args = []
        for i, part in enumerate(format_parts):
            val = args[i]
            if part[-1] == 'd':
                val = struct.unpack("<i", struct.pack("<I", val))[0]
            elif part[-1] == 'f':
                val = struct.unpack("<f", struct.pack("<I", val))[0]
            
            converted_args.append(val)
        
        try:
            return fmt % tuple(converted_args)
        except TypeError:
            return fmt + " " + repr(args)
    
    def poll(self):
        newidx = self.server.read_memory(32, self.binlog_idx, 1)
        
        if not newidx:
            print("--- Failed read")
            time.sleep(0.1)
            return
        else:
            newidx = newidx[0]
        
        if self.idx < 0:
            self.idx = max(0, newidx - self.binlog_len + 1)
        
        if newidx == self.idx:
            time.sleep(0.1)
        else:
            if newidx - self.idx >= self.binlog_len:
                print("--- Skipping from %d to %d, some messages may have been lost" % (self.idx, newidx))
                self.idx = newidx - self.binlog_len + 1
            elif newidx < self.idx:
                print("--- Skipping from %d to %d, target rebooted" % (self.idx, newidx))
                self.idx = 0
            
            data = self.server.read_memory(32, self.binlog_addr, 4 * self.binlog_len)
            
            if not data:
                print("--- Failed read")
                return
            
            while self.idx < newidx:
                pos = (self.idx & self.len_mask) * 4
                cycles, fmt, arg1, arg2 = data[pos : pos+4]
                
                if fmt not in self.strings:
                    strdata = self.server.read_memory(8, fmt, 64)
                    if 0 in strdata: strdata = strdata[:strdata.index(0)]
                    self.strings[fmt] = bytes(strdata).decode('utf-8', 'ignore')
                    
                text = self.sprintf(self.strings[fmt], (arg1, arg2))
                print("%8d %10.6f %s" % (self.idx, cycles / 38.4e6, text))
                self.idx += 1
    

if __name__ == '__main__':
    server = ocd.OpenOCD()
    symbols = nm.SymbolTable(sys.argv[1])
    logprinter = LogPrinter(server, symbols)
    
    try:
        with server:
            while True:
                logprinter.poll()
    except KeyboardInterrupt:
        pass

